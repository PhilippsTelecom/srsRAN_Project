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
 #include "srsran/cu_cp/cu_cp_types.h"
 #include "srsran/support/async/async_task.h"
 
 namespace srsran {
 
 namespace srs_cu_cp {
 
 
 /// Methods used to signal drb modification to the CU-CP.
 class drb_modifier_cu_cp_notifier
 {
 public:
   virtual ~drb_modifier_cu_cp_notifier() = default;
 
   /// \brief Notify the CU-CP about a required DRB modification
   virtual async_task<cu_cp_intra_drb_modification_response>
   on_drb_modification_required(const cu_cp_intra_drb_modification_request& request) = 0;
 };
 
 } // namespace srs_cu_cp
 
 } // namespace srsran
 