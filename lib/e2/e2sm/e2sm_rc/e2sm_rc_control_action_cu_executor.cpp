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

 #include "e2sm_rc_control_action_cu_executor.h"
 #include <future>
 #include <iostream> // JUST FOR DEBUGGING
 
 using namespace asn1::e2ap;
 using namespace asn1::e2sm;
 using namespace srsran;
 
 e2sm_rc_control_action_cu_executor_base::e2sm_rc_control_action_cu_executor_base(cu_configurator& cu_configurator_,
                                                                                  uint32_t         action_id_) :
   logger(srslog::fetch_basic_logger("E2SM-RC")), action_id(action_id_), cu_param_configurator(cu_configurator_)
 {
 }
 
 uint32_t e2sm_rc_control_action_cu_executor_base::get_action_id()
 {
  std::cout<<"[e2sm_rc_control_action_cu_executor.cpp] : Calling the function `get_action_id` "<<std::endl;
   return action_id;
 }
 
 bool e2sm_rc_control_action_cu_executor_base::fill_ran_function_description(
     asn1::e2sm::ran_function_definition_ctrl_action_item_s& action_item)
 {
   action_item.ric_ctrl_action_id = action_id;
   action_item.ric_ctrl_action_name.from_string(action_name);
 
   for (auto& ran_p : action_params) {
     ctrl_action_ran_param_item_s ctrl_action_ran_param_item;
     ctrl_action_ran_param_item.ran_param_id = ran_p.first;
     ctrl_action_ran_param_item.ran_param_name.from_string(ran_p.second);
     action_item.ran_ctrl_action_params_list.push_back(ctrl_action_ran_param_item);
   }
 
   return true;
 }
 async_task<e2sm_ric_control_response>
 e2sm_rc_control_action_cu_executor_base::return_ctrl_failure(const e2sm_ric_control_request& req)
 {
   return launch_async([req](coro_context<async_task<e2sm_ric_control_response>>& ctx) {
     CORO_BEGIN(ctx);
     e2sm_ric_control_response e2sm_response;
     e2sm_response.success                = false;
     e2sm_response.cause.set_misc().value = cause_misc_e::options::unspecified;
     CORO_RETURN(e2sm_response);
   });
 }
 

 ////////////////////////////////


 e2sm_rc_control_action_3_1_cu_executor::e2sm_rc_control_action_3_1_cu_executor(cu_configurator& cu_configurator_) :
   e2sm_rc_control_action_cu_executor_base(cu_configurator_, 1)
 {
 }
 
 bool e2sm_rc_control_action_3_1_cu_executor::ric_control_action_supported(const e2sm_ric_control_request& req)
 {
   return true;
 }
 
 async_task<e2sm_ric_control_response>
 e2sm_rc_control_action_3_1_cu_executor::execute_ric_control_action(const e2sm_ric_control_request& req)
 {
   return launch_async([this, req](coro_context<async_task<e2sm_ric_control_response>>& ctx) {
     srs_cu_cp::cu_cp_intra_cu_handover_response cu_cp_response;
     srs_cu_cp::cu_cp_intra_cu_handover_request  handover_req;
     CORO_BEGIN(ctx);
     CORO_AWAIT_VALUE(cu_cp_response,
                      cu_param_configurator.get_mobility_notifier().on_intra_cu_handover_required(
                          handover_req, srs_cu_cp::du_index_t(0), srs_cu_cp::du_index_t(1)));
     e2sm_ric_control_response e2sm_response;
     e2sm_response.success = cu_cp_response.success;
     CORO_RETURN(e2sm_response);
   });
 }
 
 void e2sm_rc_control_action_3_1_cu_executor::parse_action_ran_parameter_value(
     const asn1::e2sm::ran_param_value_type_c& ran_p,
     uint64_t                                  ran_param_id,
     uint64_t                                  ue_id,
     cu_handover_control_config&               ctrl_cfg)
 {
 }
 
 ////////////////////////////////
 
 
 // CONSTRUCTOR
 e2sm_rc_control_action_1_1_cu_executor::e2sm_rc_control_action_1_1_cu_executor(
   srsran::cu_configurator& cu_configurator_) :
 e2sm_rc_control_action_cu_executor_base(cu_configurator_, 1)
 {
   // Control Action description:
   action_name = "DRB QoS Configuration";
   action_params.insert({1, "DRB ID"});
   action_params.insert({2, "Marking Probability"}); // ECN-CE MARKING
 };
 
 // Must be there
  bool e2sm_rc_control_action_1_1_cu_executor::ric_control_action_supported(const e2sm_ric_control_request& req)
 {
   return true;
 }

 // EXECUTE ACTION
 async_task<e2sm_ric_control_response> e2sm_rc_control_action_1_1_cu_executor::execute_ric_control_action(const e2sm_ric_control_request& req)
 { 
  // Parse RIC Message -> structure ctrl_config
   srs_cu_cp::cu_cp_intra_drb_modification_request ctrl_config = convert_to_cu_config_request(req);

   // MARKING PROBABILITY IS 0 OR 100
   logger.debug("RC Action 1-1: marking probability = {}",ctrl_config.marking_prob);
   
  // So far, 1 PDU session per UE
  ctrl_config.target_pdu_index = uint_to_pdu_session_id(1); // does not work with 0
   
   // DRB MUST BE VALID
   if ( (drb_id_to_uint(ctrl_config.target_drb_index) < drb_id_to_uint(drb_id_t::drb1)) || (drb_id_to_uint(ctrl_config.target_drb_index) > drb_id_to_uint(drb_id_t::drb29)) ) {
    std::cout<<"[Action 1-1 CU executor]: DRB is invalid"<<std::endl; 
    return return_ctrl_failure(req);
   }

   // Launches asynchronous Task: PROBLEM HERE
   return launch_async([this, ctrl_config](coro_context<async_task<e2sm_ric_control_response>>& ctx) {
     srs_cu_cp::cu_cp_intra_drb_modification_response cu_cp_response;
     CORO_BEGIN(ctx);
     CORO_AWAIT_VALUE(cu_cp_response,
                      cu_param_configurator.get_drb_modifier_notifier().on_drb_modification_required(ctrl_config));
     e2sm_ric_control_response e2sm_response;
     e2sm_response.success = cu_cp_response.success;
     CORO_RETURN(e2sm_response);
   }); 
 }
 
 
 // Reads a Control Request and returns a configuration
 srs_cu_cp::cu_cp_intra_drb_modification_request
 e2sm_rc_control_action_1_1_cu_executor::convert_to_cu_config_request(const e2sm_ric_control_request& e2sm_req_)
 {
   // Structure to fill
   srs_cu_cp::cu_cp_intra_drb_modification_request ctrl_config = {};
 
   // E2SM Header and footer
   const e2sm_rc_ctrl_hdr_format1_s&   ctrl_hdr =
       std::get<e2sm_rc_ctrl_hdr_s>(e2sm_req_.request_ctrl_hdr).ric_ctrl_hdr_formats.ctrl_hdr_format1();
   const e2sm_rc_ctrl_msg_format1_s& ctrl_msg =
       std::get<e2sm_rc_ctrl_msg_s>(e2sm_req_.request_ctrl_msg).ric_ctrl_msg_formats.ctrl_msg_format1();
   
   // Retrieve UE_ID
     switch (ctrl_hdr.ue_id.type()) {
     case ue_id_c::types_opts::gnb_ue_id:
       ctrl_config.ue_id = srsran::srs_cu_cp::uint_to_ue_index(ctrl_hdr.ue_id.gnb_ue_id().amf_ue_ngap_id);
       break;
     case ue_id_c::types_opts::gnb_cu_up_ue_id:
       ctrl_config.ue_id = srsran::srs_cu_cp::uint_to_ue_index(ctrl_hdr.ue_id.gnb_cu_up_ue_id().gnb_cu_cp_ue_e1ap_id); // ran_ue_id
       break;
     default:
       ctrl_config.ue_id = srsran::srs_cu_cp::uint_to_ue_index(0);
       break;
     }
   
   // Retrieve other fields: DRB + MARKING PROBABILITY + ...
   auto parse_action_ran_parameter_value_lambda = [this](const ran_param_value_type_c&       ran_param,
                                                         uint64_t                            ran_param_id,
                                                         uint64_t                            ue_id,
                                                         srs_cu_cp::cu_cp_intra_drb_modification_request&  ctrl_cfg) {
     this->parse_action_ran_parameter_value(ran_param, ran_param_id, ue_id, ctrl_cfg);
   };
   for (auto& ran_p : ctrl_msg.ran_p_list) {
     if (action_params.find(ran_p.ran_param_id) != action_params.end()) {
       parse_ran_parameter_value(ran_p.ran_param_value_type,
                                 ran_p.ran_param_id,
                                 srsran::srs_cu_cp::ue_index_to_uint(ctrl_config.ue_id),
                                 ctrl_config,
                                 parse_action_ran_parameter_value_lambda);
     }
   }
   return ctrl_config;
 }
 
 // Used to parse an action (called for each action in the message)
 void e2sm_rc_control_action_1_1_cu_executor::parse_action_ran_parameter_value(
   const asn1::e2sm::ran_param_value_type_c&        ran_p,
   uint64_t                                         ran_param_id,
   uint64_t                                         ue_id,
   srs_cu_cp::cu_cp_intra_drb_modification_request& ctrl_cfg)
 {
 
   // Retrieve DRB (MANDATORY) : type INTEGER (1..32, ...)
   if (action_params[ran_param_id] == "DRB ID") {
    int64_t readValue = ran_p.ran_p_choice_elem_true().ran_param_value.value_int(); // int64_t
    uint8_t castValue = static_cast<uint8_t>(readValue); // uint8_t
    ctrl_cfg.target_drb_index=uint_to_drb_id(castValue); // drb_id_t
   }
   // Retrieve Marking Probability : type INTEGER (1..32, ...)
   else if(action_params[ran_param_id] == "Marking Probability") {
    logger.debug("CU action 1-1 executor: retrieving probability = {}",ran_p.ran_p_choice_elem_false().ran_param_value.value_int());
     ctrl_cfg.marking_prob = ran_p.ran_p_choice_elem_false().ran_param_value.value_int(); // int
   } 
   // Problem with current parameter
   else {
    std::cout<<"[e2sm_rc_control_action_cu_executor.cpp]: Unknown RAN parameter ID"<<std::endl;
     logger.error("Unknown RAN parameter ID {}", ran_param_id);
     return;
   }
 
 }