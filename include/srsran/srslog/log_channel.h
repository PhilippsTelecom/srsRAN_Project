/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#pragma once

#include "srsran/srslog/detail/log_backend.h"
#include "srsran/srslog/detail/log_entry.h"
#include "srsran/srslog/sink.h"
#include <atomic>

namespace srslog {

/// Type trait to indicate if a type that is going to be passed through a log channel is unsafe to be copied with the
/// default implementation and requires a user defined copy implementation.
template <typename T>
struct copy_loggable_type {
  static constexpr bool is_copyable = true;
  // static void copy (fmt::dynamic_format_arg_store<fmt::format_context>* store, <user-defined-type> a)
};

/// Log channel configuration settings.
struct log_channel_config {
  log_channel_config() = default;
  log_channel_config(std::string n, char tag_, bool should_print_context_) :
    name(std::move(n)), tag(tag_), should_print_context(should_print_context_)
  {
  }

  /// Optional log channel name. If set, will get printed for each log entry.
  /// Disabled by default.
  std::string name;
  /// Optional log channel tag. If set, will get printed for each log entry.
  /// Disabled by default.
  char tag = '\0';
  /// When set to true, each log entry will get printed with the context value.
  /// Disabled by default.
  bool should_print_context = false;
};

/// A log channel is the entity used for logging messages.
///
/// It can deliver a log entry to one or more different sinks, for example a
/// file or a console.
/// Generated log entries by the application will be ignored by the channel when
/// set to disabled.
/// NOTE: Thread safe class.
class log_channel
{
  /// This push_back implementation wrapper takes into account types that cannot be safely copied and requires user
  /// defined implementation.
  template <typename T, std::enable_if_t<copy_loggable_type<T>::is_copyable, int> = 0>
  void push_back(fmt::dynamic_format_arg_store<fmt::format_context>* store, T&& arg)
  {
    store->push_back(std::forward<T>(arg));
  }
  template <typename T, std::enable_if_t<!copy_loggable_type<T>::is_copyable, int> = 0>
  void push_back(fmt::dynamic_format_arg_store<fmt::format_context>* store, T&& arg)
  {
    copy_loggable_type<T>::copy(store, std::forward<T>(arg));
  }

  /// Context value encoding.
  static uint64_t encode_context(uint32_t a, uint32_t b) { return ((uint64_t(a) << 32) | uint64_t(b)); }

public:
  log_channel(std::string id, sink& s, detail::log_backend& backend_) : log_channel(std::move(id), s, backend_, {}) {}

  log_channel(std::string id, sink& s, detail::log_backend& backend_, log_channel_config config) :
    log_id(std::move(id)),
    log_sink(s),
    backend(backend_),
    log_name(std::move(config.name)),
    log_tag(config.tag),
    should_print_context(config.should_print_context),
    ctx_value64(0),
    hex_max_size(0),
    is_enabled(true)
  {
  }

  log_channel(const log_channel& other)            = delete;
  log_channel& operator=(const log_channel& other) = delete;

  /// Controls when the channel accepts incoming log entries.
  void set_enabled(bool enabled) { is_enabled.store(enabled, std::memory_order_relaxed); }

  /// Returns true if the channel is accepting incoming log entries, otherwise
  /// false.
  bool enabled() const { return is_enabled.load(std::memory_order_relaxed); }

  /// Returns the id string of the channel.
  const std::string& id() const { return log_id; }

  /// Set the log channel context to the specified value.
  void set_context(uint32_t a, uint32_t b) { ctx_value64 = encode_context(a, b); }

  /// Set the maximum number of bytes to can be printed in a hex dump.
  /// Set to -1 to indicate no hex dump limit.
  void set_hex_dump_max_size(int size) { hex_max_size = size; }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename... Args>
  void operator()(const char* fmtstr, Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer) {
                                 formatter.format(std::move(metadata), buffer);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {ctx_value64, should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename... Args>
  void operator()(const std::shared_ptr<const std::string>& log_label, const char* fmtstr, Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer) {
                                 formatter.format(std::move(metadata), buffer);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {ctx_value64, should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag,
                                    log_label}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename... Args>
  void operator()(uint32_t a, uint32_t b, const char* fmtstr, Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer) {
                                 formatter.format(std::move(metadata), buffer);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {encode_context(a, b), should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename... Args>
  void operator()(const uint8_t* buffer, size_t len, const char* fmtstr, Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Calculate the length to capture in the buffer.
    if (hex_max_size >= 0) {
      len = std::min<size_t>(len, hex_max_size);
    }

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer_) {
                                 formatter.format(std::move(metadata), buffer_);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {ctx_value64, should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag,
                                    {},
                                    std::vector<uint8_t>(buffer, buffer + len)}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename... Args>
  void operator()(const std::shared_ptr<const std::string>& log_label,
                  const uint8_t*                            buffer,
                  size_t                                    len,
                  const char*                               fmtstr,
                  Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Calculate the length to capture in the buffer.
    if (hex_max_size >= 0) {
      len = std::min<size_t>(len, hex_max_size);
    }

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer_) {
                                 formatter.format(std::move(metadata), buffer_);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {ctx_value64, should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag,
                                    std::move(log_label),
                                    std::vector<uint8_t>(buffer, buffer + len)}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename... Args>
  void operator()(uint32_t a, uint32_t b, const uint8_t* buffer, size_t len, const char* fmtstr, Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Calculate the length to capture in the buffer.
    if (hex_max_size >= 0) {
      len = std::min<size_t>(len, hex_max_size);
    }

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer_) {
                                 formatter.format(std::move(metadata), buffer_);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {encode_context(a, b), should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag,
                                    {},
                                    std::vector<uint8_t>(buffer, buffer + len)}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename It, typename... Args, typename std::enable_if<detail::is_byte_iterable<It>::value, int>::type = 0>
  void operator()(It it_begin, It it_end, const char* fmtstr, Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Calculate the length to capture in the buffer.
    if (hex_max_size >= 0 && hex_max_size < std::distance(it_begin, it_end)) {
      it_end = it_begin + hex_max_size;
    }

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer) {
                                 formatter.format(std::move(metadata), buffer);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {ctx_value64, should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag,
                                    {},
                                    std::vector<uint8_t>(it_begin, it_end)}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename It, typename... Args, typename std::enable_if<detail::is_byte_iterable<It>::value, int>::type = 0>
  void
  operator()(std::shared_ptr<const std::string> log_label, It it_begin, It it_end, const char* fmtstr, Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Calculate the length to capture in the buffer.
    if (hex_max_size >= 0 && hex_max_size < std::distance(it_begin, it_end)) {
      it_end = it_begin + hex_max_size;
    }

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer) {
                                 formatter.format(std::move(metadata), buffer);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {ctx_value64, should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag,
                                    log_label,
                                    std::vector<uint8_t>(it_begin, it_end)}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename... Ts>
  void operator()(const context<Ts...>& ctx)
  {
    if (!enabled()) {
      return;
    }

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter, ctx](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer) {
                                 formatter.format_ctx(ctx, std::move(metadata), buffer);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {ctx_value64, should_print_context},
                                    nullptr,
                                    nullptr,
                                    log_name,
                                    log_tag}};
    backend.push(std::move(entry));
  }

  /// Builds the provided log entry and passes it to the backend. When the
  /// channel is disabled the log entry will be discarded.
  template <typename... Ts, typename... Args>
  void operator()(const context<Ts...>& ctx, const char* fmtstr, Args&&... args)
  {
    if (!enabled()) {
      return;
    }

    // Populate the store with all incoming arguments.
    auto* store = backend.alloc_arg_store();
    if (!store) {
      return;
    }
    (void)std::initializer_list<int>{(push_back(store, std::forward<Args>(args)), 0)...};

    // Send the log entry to the backend.
    log_formatter&    formatter = log_sink.get_formatter();
    detail::log_entry entry     = {&log_sink,
                                   [&formatter, ctx](detail::log_entry_metadata&& metadata, fmt::memory_buffer& buffer) {
                                 formatter.format_ctx(ctx, std::move(metadata), buffer);
                               },
                                   {std::chrono::high_resolution_clock::now(),
                                    {ctx_value64, should_print_context},
                                    fmtstr,
                                    store,
                                    log_name,
                                    log_tag}};
    backend.push(std::move(entry));
  }

private:
  const std::string     log_id;
  sink&                 log_sink;
  detail::log_backend&  backend;
  const std::string     log_name;
  const char            log_tag;
  const bool            should_print_context;
  std::atomic<uint64_t> ctx_value64;
  std::atomic<int>      hex_max_size;
  std::atomic<bool>     is_enabled;
};

} // namespace srslog
