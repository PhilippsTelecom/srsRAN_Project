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

#include "../pdcch_scheduling/pdcch_resource_allocator.h"
#include "srsran/ran/slot_point.h"
#include "srsran/scheduler/config/scheduler_expert_config.h"
#include "srsran/scheduler/sched_consts.h"
#include "srsran/srslog/srslog.h"

namespace srsran {

struct cell_slot_resource_allocator;
enum class ssb_pattern_case;

/// This class schedules the SIB1 and fills the grant to be passed to lower layers.
class sib1_scheduler
{
public:
  sib1_scheduler(const scheduler_si_expert_config&               expert_cfg_,
                 const cell_configuration&                       cfg_,
                 pdcch_resource_allocator&                       pdcch_sch,
                 const sched_cell_configuration_request_message& msg);

  /// \brief Schedule the SIB1 grants.
  ///
  /// The first time this function gets called, it allocates the SIB1 grants all over the grid, according to the
  /// periodicity and according to each beam's slot. From the second time on, it only allocates the SIB1 grants (if any)
  /// in the latest available slot of the grid.
  ///
  /// \param[out] res_alloc  Cell resource grid.
  /// \param[in]  sl_point   Slot point carrying information about current slot.
  void run_slot(cell_resource_allocator& res_alloc, const slot_point& sl_point);

  /// \brief Update the SIB1 PDU version.
  void handle_sib1_update_indication(unsigned version, units::bytes sib1_payload_size);

private:
  /// \brief Performs beams' SIB1s (if any) scheduling for the current slot.
  ///
  /// \param[out,in] res_grid Resource grid with current allocations and scheduling results.
  /// \param[in] sl_point Slot for which the SIB1 scheduler is called.
  void schedule_sib1(cell_slot_resource_allocator& res_grid);

  /// \brief Searches in PDSCH and PDCCH for space to allocate SIB1 and SIB1's DCI, respectively.
  ///
  /// \param[out,in] res_grid Resource grid with current allocations and scheduling results.
  /// \param[in] beam_idx SSB or beam index which the SIB1 corresponds to.
  /// \param[in] time_resource PDSCH time domain resource.
  bool allocate_sib1(cell_slot_resource_allocator& res_grid, unsigned beam_idx, unsigned time_resource);

  /// \brief Fills the SIB1 slots, at which each beam's SIB1 is allocated.
  ///
  /// These slots are computed and saved in the body of the constructor.
  /// \param[out,in] res_grid Resource grid with current allocations and scheduling results.
  /// \param[in] sib1_crbs_grant CRBs interval in the PDSCH allocated for SIB1.
  /// \param[in] time_resource PDSCH time domain resource.
  /// \param[in] dmrs_info DMRS information for SIB1.
  /// \param[in] tbs Transport block size.
  void fill_sib1_grant(cell_slot_resource_allocator& res_grid,
                       crb_interval                  sib1_crbs_grant,
                       unsigned                      time_resource,
                       const dmrs_information&       dmrs_info,
                       unsigned                      tbs);

  void handle_pending_sib1_update();

  /// SIB1 Logger.
  srslog::basic_logger& logger = srslog::fetch_basic_logger("SCHED");

  /// Parameters for SIB1 scheduling.
  const scheduler_si_expert_config& expert_cfg;
  const cell_configuration&         cell_cfg;
  pdcch_resource_allocator&         pdcch_sched;
  bool                              first_run_slot = true;

  /// Parameters for SIB1 scheduling.
  uint8_t coreset0;
  uint8_t searchspace0;
  /// This is a derived parameters, that depends on the SSB periodicity, SIB1 periodicity and SIB1 re-tx periodicity.
  std::chrono::milliseconds sib1_rtx_period;
  /// The SIB1 payload is in bytes.
  units::bytes sib1_payload_size;
  /// Current SIB version.
  unsigned current_version = 0;

  /// This is a dummy BWP configuration dimensioned based on CORESET#0 RB limits. It's used for CRB-to-PRB conversion.
  bwp_configuration coreset0_bwp_cfg;

  /// Array of Type0-PDCCH CSS slots  (1 per beam) that will be used for SIB1 scheduling [TS 38.213, Section 13].
  std::array<slot_point, MAX_NUM_BEAMS> sib1_type0_pdcch_css_slots;

  /// Pending new SIB1 PDU to be applied.
  std::atomic<uint64_t> pending_update;
};

} // end of namespace srsran
