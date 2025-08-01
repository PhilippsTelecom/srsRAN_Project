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

#include "rlc_sdu_queue_lockfree.h"
#include "rlc_tx_entity.h"
#include "srsran/support/executors/task_executor.h"
#include "fmt/format.h"
#include <mutex>

namespace srsran {

///
/// \brief TX state variables
/// Ref: 3GPP TS 38.322 version 16.2.0 Section 7.1
///
struct rlc_tx_um_state {
  ///
  /// \brief  TX_Next – UM send state variable
  /// It holds the value of the SN to be assigned for the next newly generated UMD PDU with
  /// segment. It is initially set to 0, and is updated after the UM RLC entity submits a UMD PDU
  /// including the last segment of an RLC SDU to lower layers.
  ///
  uint32_t tx_next = 0;
};

class rlc_tx_um_entity : public rlc_tx_entity
{
private:
  // Config storage
  const rlc_tx_um_config cfg;

  // TX state variables
  rlc_tx_um_state st;

  // TX SDU buffers
  rlc_sdu_queue_lockfree sdu_queue;
  rlc_sdu                sdu;
  uint32_t               next_so = 0; // The segment offset for the next generated PDU

  /// TX counter modulus
  const uint32_t mod;

  // Header sizes are computed upon construction based on SN length
  const uint32_t head_len_full;
  const uint32_t head_len_first;
  const uint32_t head_len_not_first;

  pcap_rlc_pdu_context pcap_context;

  // Storage for previous buffer state
  rlc_buffer_state prev_buffer_state = {};

  /// This atomic_flag indicates whether a buffer state update task has been queued but not yet run by pcell_executor.
  /// It helps to avoid queuing of redundant notification tasks in case of frequent changes of the buffer status.
  /// If the flag is set, no further notification needs to be scheduled, because the already queued task will pick the
  /// latest buffer state upon execution.
  std::atomic_flag pending_buffer_state = ATOMIC_FLAG_INIT;

public:
  rlc_tx_um_entity(gnb_du_id_t                          gnb_du_id,
                   du_ue_index_t                        ue_index,
                   rb_id_t                              rb_id_,
                   const rlc_tx_um_config&              config,
                   rlc_tx_upper_layer_data_notifier&    upper_dn_,
                   rlc_tx_upper_layer_control_notifier& upper_cn_,
                   rlc_tx_lower_layer_notifier&         lower_dn_,
                   rlc_metrics_aggregator&              metrics_agg_,
                   rlc_pcap&                            pcap_,
                   task_executor&                       pcell_executor_,
                   task_executor&                       ue_executor_,
                   timer_manager&                       timers);

  void stop() final
  {
    // Stop all timers. Any queued handlers of timers that just expired before this call are canceled automatically
    if (not stopped) {
      high_metrics_timer.stop();
      low_metrics_timer.stop();
      stopped = true;
    }
  }

  // Interfaces for higher layers
  void handle_sdu(byte_buffer sdu_buf, bool is_retx) override;
  void discard_sdu(uint32_t pdcp_sn) override;

  // Interfaces for lower layers
  size_t           pull_pdu(span<uint8_t> mac_sdu_buf) override;
  rlc_buffer_state get_buffer_state() override;

private:
  bool get_si_and_expected_header_size(uint32_t      so,
                                       uint32_t      sdu_len,
                                       uint32_t      grant_len,
                                       rlc_si_field& si,
                                       uint32_t&     head_len) const;

  /// Called whenever the buffer state has been changed by upper layers (new SDUs or SDU discard) so that lower layers
  /// need to be informed about the new buffer state. This function defers the actual notification \c
  /// handle_changed_buffer_state to pcell_executor. The notification is discarded if another notification is already
  /// queued for execution. This function should not be called from \c pull_pdu, since the lower layer accounts for the
  /// amount of extracted data itself.
  ///
  /// Safe execution from: Any executor
  void handle_changed_buffer_state();

  /// Immediately informs the lower layer of the current buffer state. This function is called from pcell_executor and
  /// its execution is queued by \c handle_changed_buffer_state.
  ///
  /// Safe execution from: pcell_executor
  void update_mac_buffer_state();

  void log_state(srslog::basic_levels level) { logger.log(level, "TX entity state. {} next_so={}", st, next_so); }

  void update_queue_length_metrics();

  bool stopped = false;
};

} // namespace srsran

namespace fmt {
template <>
struct formatter<srsran::rlc_tx_um_state> {
  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const srsran::rlc_tx_um_state& st, FormatContext& ctx) const
  {
    return format_to(ctx.out(), "tx_next={}", st.tx_next);
  }
};

} // namespace fmt
