#pragma once

#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include "srsran/srslog/srslog.h"

class RIC {
public:
  explicit RIC(const std::string& socket_path = "");
  ~RIC();

  // Setup socket (called by constructor). Returns 0 on success, non-zero on error.
  int setup_socket();

  // Non-blocking: enqueue message to be sent by background thread.
  // If the internal queue is full or busy the message may be dropped to avoid introducing latency.
  void send_message(const std::string& msg);

  // Blocking/direct send: performs sendto() in the caller thread.
  void send_message_direct(const std::string& msg);

private:
  int sock_fd = -1;
  struct sockaddr_un addr{};
  std::string socket_path_;

  // srsRAN logger
  srslog::basic_logger& logger;

  // Asynchronous send infrastructure
  std::thread sender_thread;
  std::mutex queue_mutex;
  uint32_t min_size = 30;
  std::condition_variable queue_cv;
  std::deque<std::string> msg_queue;
  std::atomic<bool> running{false};
  size_t max_queue_size = 1024 * 20;

  void sender_thread_loop();
  bool send_to_socket(const std::string& msg);
};