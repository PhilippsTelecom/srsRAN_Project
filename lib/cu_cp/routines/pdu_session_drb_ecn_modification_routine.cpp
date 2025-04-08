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

 #include "pdu_session_drb_ecn_modification_routine.h"
 #include "pdu_session_routine_helpers.h"
 #include "srsran/cu_cp/ue_task_scheduler.h"
 #include "srsran/ran/cause/e1ap_cause_converters.h"
 #include <iostream> // JUST FOR DEBUGGING
 
 using namespace srsran;
 using namespace srsran::srs_cu_cp;
 using namespace asn1::rrc_nr;
 
 // Free functions to amend to the final procedure response message. This will take the results from the various
 // sub-procedures and update the succeeded/failed fields.
 
 /// \brief Handle first Bearer Context Modification response and prepare subsequent UE context modification request.
 bool handle_bearer_context_modification_response(
     cu_cp_intra_drb_modification_response&      response_msg,
     const cu_cp_intra_drb_modification_request& modify_request,
     const e1ap_bearer_context_modification_response& bearer_context_modification_response,
     up_config_update&                                next_config,
     srslog::basic_logger&                            logger);
 
 pdu_session_drb_ecn_modification_routine::pdu_session_drb_ecn_modification_routine(
     const cu_cp_intra_drb_modification_request& modify_request_,
     e1ap_bearer_context_manager&                     e1ap_bearer_ctxt_mng_,
     up_resource_manager&                             up_resource_mng_,
     srslog::basic_logger&                            logger_) :
   modify_request(modify_request_),
   e1ap_bearer_ctxt_mng(e1ap_bearer_ctxt_mng_),
   up_resource_mng(up_resource_mng_),
   logger(logger_)
 {
 }
 
 void pdu_session_drb_ecn_modification_routine::operator()(
     coro_context<async_task<cu_cp_intra_drb_modification_response>>& ctx)
 {
   CORO_BEGIN(ctx);
 
   logger.debug("ue={}: \"{}\" initialized", modify_request.ue_id, name());
 
   // Perform initial sanity checks on incoming message.
   if (!up_resource_mng.validate_request(modify_request)) {
      std::cout<<"[pdu_session_drb_ecn_modification_routine.cpp]: problems validating request "<<std::endl;
     logger.warning("ue={}: \"{}\" Invalid PduSessionResourceModification", modify_request.ue_id, name());
     CORO_EARLY_RETURN(generate_pdu_session_resource_modify_response(false));
   }
 
   {
     // Calculate next user-plane configuration based on incoming modify message.
     next_config = up_resource_mng.calculate_update(modify_request);
   }
 
   {
     // Prepare BearerContextModificationRequest.
     bearer_context_modification_request.ng_ran_bearer_context_mod_request.emplace(); // Initialize fresh message
     fill_initial_e1ap_bearer_context_modification_request(bearer_context_modification_request); //  Transforms in a E1AP request

     // Call E1AP procedure and wait for BearerContextModificationResponse.
     CORO_AWAIT_VALUE(
         bearer_context_modification_response,
         e1ap_bearer_ctxt_mng.handle_bearer_context_modification_request(bearer_context_modification_request)); 

 
     // Handle BearerContextModificationResponse and fill subsequent UEContextModificationRequest.
     if (!handle_bearer_context_modification_response(response_msg,
                                                      modify_request,
                                                      bearer_context_modification_response,
                                                      next_config,
                                                      logger)) {
      std::cout<<"[pdu_session_drb_ecn_modification_routine.cpp]: failed modifying bearer at CU-UP"<<std::endl;
       logger.warning("ue={}: \"{}\" failed to modify bearer at CU-UP", modify_request.ue_id, name());
       CORO_EARLY_RETURN(generate_pdu_session_resource_modify_response(false));// Update context
     }
   }

   // We are done.
   CORO_RETURN(generate_pdu_session_resource_modify_response(true));// Update context
 }
 
//  void fill_e1ap_pdu_session_res_to_modify_list(
//      slotted_id_vector<pdu_session_id_t, e1ap_pdu_session_res_to_modify_item>& pdu_session_res_to_modify_list,
//      srslog::basic_logger&                                                     logger,
//      const up_config_update&                                                   next_config,
//      const cu_cp_intra_drb_modification_request&                          modify_request)
//  {
//    for (const auto& modify_item : next_config.pdu_sessions_to_modify_list) {
//      const auto& session = modify_item.second;
//      srsran_assert(modify_request.pdu_session_res_modify_items.contains(session.id),
//                    "Modify request doesn't contain config for {}",
//                    session.id);
 
//      // Obtain PDU session config from original resource modify request.
//      const auto&                         pdu_session_cfg = modify_request.pdu_session_res_modify_items[session.id];
//      e1ap_pdu_session_res_to_modify_item e1ap_pdu_session_item;
//      e1ap_pdu_session_item.pdu_session_id = session.id;
//     //  fill_drb_to_setup_list(e1ap_pdu_session_item.drb_to_setup_list_ng_ran,
//     //                         pdu_session_cfg.transfer.qos_flow_add_or_modify_request_list,
//     //                         session.drb_to_add,
//     //                         logger);
 
//      fill_drb_to_modify_list(e1ap_pdu_session_item.drb_to_modify_list_ng_ran,
//                              pdu_session_cfg.transfer.qos_flow_add_or_modify_request_list,
//                              session.drb_to_modify,
//                              logger);
 
//     //  fill_drb_to_remove_list(e1ap_pdu_session_item.drb_to_rem_list_ng_ran, session.drb_to_remove);
 
//      pdu_session_res_to_modify_list.emplace(pdu_session_cfg.pdu_session_id, e1ap_pdu_session_item);
//    }
//  }
 
 // Helper to fill a Bearer Context Modification request if it is the initial E1AP message
 // for this procedure.
 void pdu_session_drb_ecn_modification_routine::fill_initial_e1ap_bearer_context_modification_request(
     e1ap_bearer_context_modification_request& e1ap_request)
 {
    // `e1ap_bearer_context_modification_request` (0)
   e1ap_request.ue_index = modify_request.ue_id; // FILL UE ID

   // `e1ap_ng_ran_bearer_context_mod_request` : CONTEXT MODIFICATION REQUEST (1)
   e1ap_request.ng_ran_bearer_context_mod_request.emplace();
   
   // `e1ap_pdu_session_res_to_modify_item` : PDU LEVEL (2)
   pdu_session_id_t pdu_id = modify_request.target_pdu_index;
   e1ap_pdu_session_res_to_modify_item e1ap_pdu_session_item;
   e1ap_pdu_session_item.pdu_session_id = pdu_id;// FILL PDU SESSION ID

   // `e1ap_drb_to_modify_item_ng_ran` : DRB LEVEL (3)
   drb_id_t drb_id = modify_request.target_drb_index;
   e1ap_drb_to_modify_item_ng_ran e1ap_drb_item;
   e1ap_drb_item.drb_id = drb_id; // FILL DRB ID
   // Retrieve values from `next_config`
   up_drb_context drb_ctx = next_config.pdu_sessions_to_modify_list.at(pdu_id).drb_to_modify.at(drb_id);
   
   // SDAP LEVEL (4)
   e1ap_drb_item.sdap_cfg = drb_ctx.sdap_cfg; // COPY SDAP
   // PDCP LEVEL (4)
   e1ap_drb_item.pdcp_cfg.emplace(); 
   fill_e1ap_drb_pdcp_config(e1ap_drb_item.pdcp_cfg.value(),drb_ctx.pdcp_cfg); // COPY PDCP: PUT (4) IN (3)
   
   // FLOW MAP LEVEL (4) : IGNORED

   // BUILD <> Russian doll
   e1ap_pdu_session_item.drb_to_modify_list_ng_ran.emplace(drb_id,e1ap_drb_item); // PUT (3) IN (2)
   e1ap_request.ng_ran_bearer_context_mod_request.value().pdu_session_res_to_modify_list.emplace(pdu_id,e1ap_pdu_session_item); // PUT (2) IN (1)
 }
 
 /// \brief Processes the result of a Bearer Context Modification Result's PDU session modify list.
 /// \param[in] bearer_context_modification_response Const reference to the response of the previous subprocedure.
 /// \param[in] next_config Const reference to the calculated config update.
 /// \param[in] logger Reference to the logger.
 /// \return True on success, false otherwise.
 static bool update_modify_list_with_bearer_ctxt_mod_response(
     cu_cp_intra_drb_modification_response            response_msg,
     const e1ap_bearer_context_modification_response& bearer_context_modification_response,
     const cu_cp_intra_drb_modification_request&      modify_request,
     up_config_update&                                next_config,
     const srslog::basic_logger&                      logger)
 {
   for (const auto& e1ap_item : bearer_context_modification_response.pdu_session_resource_modified_list) {
     const auto& psi = e1ap_item.pdu_session_id;
     // Sanity check - make sure this session ID is present in the original modify message.
     if (! (modify_request.target_pdu_index==psi) ) {
      std::cout<<"[pdu_session_drb_ecn_modification_routine.cpp]: session ID was not present in initial request"<<std::endl;
       logger.warning("PduSessionResourceSetupRequest doesn't include setup for {}", psi);
       return false;
     }
     // Also check if PDU session is included in expected next configuration.
     if (next_config.pdu_sessions_to_modify_list.find(psi) == next_config.pdu_sessions_to_modify_list.end()) {
      std::cout<<"[pdu_session_drb_ecn_modification_routine.cpp]: PDU session ID was not present in initial request"<<std::endl;
       logger.warning("Didn't expect modification for {}", psi);
       return false;
     }
 
     for (const auto& e1ap_drb_item : e1ap_item.drb_setup_list_ng_ran) {
       const auto& drb_id = e1ap_drb_item.drb_id;
       // Check if modified DRB is part of next config
       if (next_config.pdu_sessions_to_modify_list.at(psi).drb_to_add.find(drb_id) ==
           next_config.pdu_sessions_to_modify_list.at(psi).drb_to_add.end()) {
            std::cout<<"[pdu_session_drb_ecn_modification_routine.cpp]: DRB ID is not part of next config"<<std::endl;
         logger.warning("{} not part of next configuration", drb_id);
         return false;
       }
     }
 
     // Fail on any DRB that fails to be setup.
     if (!e1ap_item.drb_failed_list_ng_ran.empty()) {
      std::cout<<"[pdu_session_drb_ecn_modification_routine.cpp]: non-empty DRB failed list not supported"<<std::endl;
       logger.warning("Non-empty DRB failed list not supported");
       return false;
     }

     // Everything is OKAY: edit structure to send back to E2 agent
     std::cout<<"[pdu_session_drb_ecn_modification_routine.cpp]: setting success in response message"<<std::endl;
     response_msg.success = true;
   }
 
   return true;
 }
 
 /// \brief Processes the response of a Bearer Context Modification Request.
 /// \param[out] response_msg Reference to the final NGAP response.
 /// \param[out] next_config Const reference to the calculated config update.
 /// \param[in] pdu_session_resource_failed_list Const reference to the failed PDU sessions of the Bearer Context
 /// Modification Response.
 static void update_failed_list_with_bearer_ctxt_mod_response(
     up_config_update&                                                                 next_config,
     const slotted_id_vector<pdu_session_id_t, e1ap_pdu_session_resource_failed_item>& pdu_session_resource_failed_list)
 {
   for (const auto& e1ap_item : pdu_session_resource_failed_list) {
     // Remove from next config.
     next_config.pdu_sessions_to_setup_list.erase(e1ap_item.pdu_session_id);
   }
 }
 
 // \brief Handle BearerContextModificationResponse
 bool handle_bearer_context_modification_response(
      cu_cp_intra_drb_modification_response&      response_msg,
     const cu_cp_intra_drb_modification_request& modify_request,
     const e1ap_bearer_context_modification_response& bearer_context_modification_response,
     up_config_update&                                next_config,
     srslog::basic_logger&                            logger)
 {
     // Some sanity checks
     if (!update_modify_list_with_bearer_ctxt_mod_response(
      response_msg,
      bearer_context_modification_response,
      modify_request,
      next_config,
      logger)) {
        std::cout<<"[pdu_session_drb_ecn_modification_routine.cpp]: handling E1AP response, problem with sanity checks"<<std::endl;
      return false;
      } 

    // Update Next config depending of modification success
    update_failed_list_with_bearer_ctxt_mod_response(
      next_config, bearer_context_modification_response.pdu_session_resource_failed_list);
  
   return bearer_context_modification_response.success;
 }
 
 // Helper to mark all PDU sessions that were requested to be set up as failed.
//  void mark_all_sessions_as_failed(cu_cp_pdu_session_resource_modify_response&      response_msg,
//                                   const cu_cp_intra_drb_modification_request& modify_request)
//  {
//    for (const auto& modify_item : modify_request.pdu_session_res_modify_items) {
//      cu_cp_pdu_session_resource_failed_to_modify_item failed_item;
//      failed_item.pdu_session_id              = modify_item.pdu_session_id;
//      failed_item.unsuccessful_transfer.cause = ngap_cause_radio_network_t::unspecified;
//      response_msg.pdu_session_res_failed_to_modify_list.emplace(failed_item.pdu_session_id, failed_item);
//    }
//    // No PDU session modified can be successful at the same time.
//    response_msg.pdu_session_res_modify_list.clear();
//  }
 
 // Echec <=> toutes les sessions ont echoue durant leur modification
 // Succes: 
  // -> MAJ du contexte pour les sessions modifiees
  // -> MSG config update pour les sessions echouees
  cu_cp_intra_drb_modification_response
 pdu_session_drb_ecn_modification_routine::generate_pdu_session_resource_modify_response(bool success)
 {
  cu_cp_intra_drb_modification_response response;

   if (success) {
     logger.debug("ue={}: \"{}\" finalized", modify_request.ue_id, name());
 
     // Prepare update for UP resource manager.
     up_config_update_result result;
     for (const auto& pdu_session_to_mod : next_config.pdu_sessions_to_modify_list) {
       result.pdu_sessions_modified_list.push_back(pdu_session_to_mod.second);
     }

     // Update context
     up_resource_mng.apply_config_update(result);

   } else {
     logger.warning("ue={}: \"{}\" failed", modify_request.ue_id, name());
     //mark_all_sessions_as_failed(response_msg, modify_request);
   }
   return response_msg;
 }
 