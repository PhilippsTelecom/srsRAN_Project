#include "ric.h"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <cerrno>

RIC::RIC(const std::string& socket_path) : socket_path_(socket_path), logger(srslog::fetch_basic_logger("RIC"))
{
  sock_fd = -1;
  std::memset(&addr, 0, sizeof(addr));

  if (socket_path_.empty()) {
    const char* env_path = std::getenv("IPC_SOCKET_PATH");
    if (env_path && env_path[0] != '\0') {
      socket_path_ = env_path;
      logger.warning("RIC IPC socket_path not provided, using IPC_SOCKET_PATH from env: {}", socket_path_);
    } else {
      socket_path_ = "/home/amirmo/testbed/video/ecn-pion/testbed/ipc.sock";
      logger.error("RIC IPC socket_path not provided and IPC_SOCKET_PATH env empty; falling back to default: {}", socket_path_);
    }
  } else {
    logger.info("RIC constructed with socket path: {}", socket_path_);
  }

  (void)setup_socket();

  // Start background sender
  // running.store(true);
  // sender_thread = std::thread(&RIC::sender_thread_loop, this);
}

RIC::~RIC()
{
  // Stop background sender
  running.store(false);
  queue_cv.notify_all();
  if (sender_thread.joinable()) {
    sender_thread.join();
  }

  if (sock_fd >= 0) {
    logger.info("Closing RIC socket fd={}", sock_fd);
    ::close(sock_fd);
    sock_fd = -1;
  }
}

int RIC::setup_socket()
{
  sock_fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
  if (sock_fd < 0) {
    logger.error("socket() failed: {}", std::strerror(errno));
    return 1;
  }

  addr = {};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
  logger.info("RIC socket setup complete, path={}", socket_path_);
  return 0;
}

bool RIC::send_to_socket(const std::string& msg)
{
  if (sock_fd < 0) {
    logger.error("send_to_socket: invalid socket");
    return false;
  }

  ssize_t ret = ::sendto(sock_fd, msg.data(), msg.size(), 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
  if (ret < 0) {
    logger.error("sendto() failed: {}", std::strerror(errno));
    return false;
  }
  return true;
}

void RIC::send_message_direct(const std::string &msg)
{
  if (send_to_socket(msg)) {
    logger.debug("RIC sent message ({} bytes) to {} [direct]", msg.size(), socket_path_);
  }
}

void RIC::send_message(const std::string &msg)
{
  // Non-blocking: try to acquire the lock and push the message.
  // If the mutex is contended or the queue is full, drop the message to avoid latency.
  if (!queue_mutex.try_lock()) {
    logger.warning("RIC queue busy, dropping message to avoid latency");
    return;
  }

  bool pushed = false;
  if (msg_queue.size() < max_queue_size) {
    msg_queue.emplace_back(msg);
    if (msg_queue.size() > min_size) {
        pushed = true;
    }
  }
  queue_mutex.unlock();

  if (pushed) {
    queue_cv.notify_one();
    logger.debug("RIC enqueued message ({} bytes) to {}", msg.size(), socket_path_);
  } else {
    logger.warning("RIC message queue full, dropping message");
  }
}

void RIC::sender_thread_loop()
{
  while (running.load() || !msg_queue.empty()) {
    std::unique_lock<std::mutex> lk(queue_mutex);
    queue_cv.wait(lk, [&] { return !msg_queue.empty() || !running.load(); });

    while (!msg_queue.empty()) {
      std::string msg = std::move(msg_queue.front());
      msg_queue.pop_front();
      lk.unlock();

      bool ok = send_to_socket(msg);
      if (!ok) {
        logger.error("Failed to send enqueued message ({} bytes) to {}", msg.size(), socket_path_);
      } else {
        logger.debug("RIC sent enqueued message ({} bytes) to {}", msg.size(), socket_path_);
      }

      // Re-lock to check for more messages
      lk.lock();
    }
  }
}