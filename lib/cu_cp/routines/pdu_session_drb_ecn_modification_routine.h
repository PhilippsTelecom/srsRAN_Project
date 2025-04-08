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

 #include "../cu_cp_impl_interface.h"
 #include "../du_processor/du_processor.h"
 #include "../up_resource_manager/up_resource_manager_impl.h"
 #include "srsran/cu_cp/ue_task_scheduler.h"
 #include "srsran/e1ap/cu_cp/e1ap_cu_cp.h"
 #include "srsran/support/async/async_task.h"
 
 namespace srsran {
 namespace srs_cu_cp {
 

 /// \brief Handles the modification of an existing DRB => change ECN-CE marking probability
 class pdu_session_drb_ecn_modification_routine
 {
 public:
   pdu_session_drb_ecn_modification_routine(const cu_cp_intra_drb_modification_request& modify_request_,
                                             e1ap_bearer_context_manager&                     e1ap_bearer_ctxt_mng_,
                                             up_resource_manager&                             up_resource_mng_,
                                             srslog::basic_logger&                            logger_);
 
   void operator()(coro_context<async_task<cu_cp_intra_drb_modification_response>>& ctx);
 
   static const char* name() { return "DRB Modification Routine"; }
 
 private:
   //void fill_e1ap_bearer_context_modification_request(e1ap_bearer_context_modification_request& e1ap_request);
   void fill_initial_e1ap_bearer_context_modification_request(e1ap_bearer_context_modification_request& e1ap_request);
 
   cu_cp_intra_drb_modification_response generate_pdu_session_resource_modify_response(bool success);
 

   const cu_cp_intra_drb_modification_request modify_request; // the original request
   up_config_update next_config; // the derivated request
 

   e1ap_bearer_context_manager& e1ap_bearer_ctxt_mng; // to trigger bearer context setup at CU-UP
   up_resource_manager&         up_resource_mng;      // to get RRC DRB config
   srslog::basic_logger&        logger;
 
   // (sub-)routine requests
   e1ap_bearer_context_modification_request bearer_context_modification_request;
 
   // (sub-)routine results
   cu_cp_intra_drb_modification_response response_msg;                         // Final routine result.
   e1ap_bearer_context_modification_response bearer_context_modification_response; // to inform CU-UP about the new TEID for UL F1u traffic
 };
 
 } // namespace srs_cu_cp
 } // namespace srsran
 