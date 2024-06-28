/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "apps/gnb/gnb_appconfig.h"
#include "apps/services/logger/logger_appconfig.h"
#include <string>

namespace srsran {
namespace srs_cu {

/// NR-U configuration
struct cu_nru_appconfig {
  std::string bind_addr       = "127.0.10.1"; // Bind address used by the F1-U interface
  int         udp_rx_max_msgs = 256; // Max number of UDP packets received by a single syscall on the F1-U interface.
};

/// F1AP configuration
struct cu_f1ap_appconfig {
  /// F1-C bind address
  std::string bind_addr = "127.0.10.1";
};

} // namespace srs_cu

/// Monolithic gnb application configuration.
struct cu_appconfig {
  /// Default constructor to update the log filename.
  cu_appconfig() { log_cfg.filename = "/tmp/cu.log"; }

  /// Loggers configuration.
  logger_appconfig log_cfg;

  /// Expert configuration.
  expert_execution_appconfig expert_execution_cfg;

  /// NR-U
  srs_cu::cu_nru_appconfig nru_cfg;

  /// F1AP
  srs_cu::cu_f1ap_appconfig f1ap_cfg;

  /// Buffer pool configuration.
  buffer_pool_appconfig buffer_pool_config;

  /// TODO fill in the rest of the configuration
};

} // namespace srsran
