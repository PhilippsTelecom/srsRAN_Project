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

#include "e2_subscription_manager_impl.h"
#include "srsran/asn1/e2ap/e2ap.h"

#include <iostream> // Just for debugging

using namespace asn1::e2ap;
using namespace srsran;

#define E2SM_IFACE(ran_function_id_value) e2sm_iface_list[supported_ran_functions[ran_function_id_value]]

e2_subscription_manager_impl::e2_subscription_manager_impl(e2sm_manager& e2sm_mngr_) :
  e2sm_mngr(e2sm_mngr_), logger(srslog::fetch_basic_logger("E2-SUBSCRIBER"))
{
}

void e2_subscription_manager_impl::stop()
{
  for (auto& c : subscriptions) {
    c.second.indication_task.await_ready();
  }
  subscriptions.clear();
}

e2_subscribe_reponse_message
e2_subscription_manager_impl::handle_subscription_setup(const asn1::e2ap::ric_sub_request_s& msg)
{
  e2_subscription_t            subscription = {};
  e2_subscribe_reponse_message outcome;
  subscription.subscription_info.request_id.ric_requestor_id = msg->ric_request_id.ric_requestor_id;
  subscription.subscription_info.request_id.ric_instance_id  = msg->ric_request_id.ric_instance_id;
  subscription.subscription_info.ran_function_id             = msg->ran_function_id;
  e2sm_event_trigger_definition event_trigger_def;

  if (supported_ran_functions.count(msg->ran_function_id)) {
    e2sm_interface* e2sm = e2sm_mngr.get_e2sm_interface(msg->ran_function_id);
    if (e2sm == nullptr) {
      logger.error("Failed to get E2SM interface, RAN function {} not in allowed list", msg->ran_function_id);
      return outcome;
    }
    event_trigger_def = e2sm->get_e2sm_packer().handle_packed_event_trigger_definition(
        msg->ric_sub_details.ric_event_trigger_definition);
    subscription.subscription_info.report_period = event_trigger_def.report_period;
    std::cout<<"[e2_subscription_manager_impl.cpp]: handle_subscription_setup _ report_period = "<<subscription.subscription_info.report_period<<std::endl; 
    outcome.request_id.ric_requestor_id          = subscription.subscription_info.request_id.ric_requestor_id;
    outcome.request_id.ric_instance_id           = subscription.subscription_info.request_id.ric_instance_id;
    outcome.ran_function_id                      = subscription.subscription_info.ran_function_id;
    subscriptions.insert(std::pair<e2_subscription_key_t, e2_subscription_t>(
        std::make_tuple(subscription.subscription_info.request_id.ric_requestor_id,
                        subscription.subscription_info.request_id.ric_instance_id),
        std::move(subscription)));
    get_subscription_result(msg->ran_function_id,
                            outcome,
                            subscriptions[{outcome.request_id.ric_requestor_id, outcome.request_id.ric_instance_id}],
                            msg->ric_sub_details.ric_action_to_be_setup_list);
    if (!outcome.success) {
      logger.error("Failed to setup subscription");
      subscriptions.erase({outcome.request_id.ric_requestor_id, outcome.request_id.ric_instance_id});
    }
  } else {
    outcome.request_id.ric_requestor_id = subscription.subscription_info.request_id.ric_requestor_id;
    outcome.request_id.ric_instance_id  = subscription.subscription_info.request_id.ric_instance_id;
    outcome.success                     = false;
    outcome.cause.set_protocol();
    logger.error("RAN function ID={} not supported", msg->ran_function_id);
  }
  return outcome;
}

e2_subscribe_delete_response_message
e2_subscription_manager_impl::handle_subscription_delete(const asn1::e2ap::ric_sub_delete_request_s& msg)
{
  e2_subscribe_delete_response_message outcome = {};
  outcome.request_id.ric_requestor_id          = msg->ric_request_id.ric_requestor_id;
  outcome.request_id.ric_instance_id           = msg->ric_request_id.ric_instance_id;
  outcome.response->ran_function_id            = msg->ran_function_id;
  outcome.response->ric_request_id             = msg->ric_request_id;
  outcome.success                              = false;
  if (subscriptions.count({outcome.request_id.ric_requestor_id, outcome.request_id.ric_instance_id})) {
    outcome.success = true;
  } else {
    outcome.failure->cause.set_misc();
    logger.error("RIC instance ID not found");
  }
  return outcome;
}

void e2_subscription_manager_impl::start_subscription(const asn1::e2ap::ric_request_id_s& ric_request_id,
                                                      uint16_t                            ran_func_id,
                                                      e2_event_manager&                   ev_mng,
                                                      e2_message_notifier&                tx_pdu_notifier)
{
  e2sm_interface* e2sm = e2sm_mngr.get_e2sm_interface(ran_func_id);
  for (auto& action :
       subscriptions[{ric_request_id.ric_requestor_id, ric_request_id.ric_instance_id}].subscription_info.action_list) {
    auto& action_def = action.action_definition;
    if (action.ric_action_type == asn1::e2ap::ric_action_type_e::report) {
      action.report_service = e2sm->get_e2sm_report_service(action_def);
      if (action.report_service == nullptr) {
        logger.error("Failed to get E2SM report service for RAN function ID {}", ran_func_id);
        return;
      }
    }
  }

  subscriptions[{ric_request_id.ric_requestor_id, ric_request_id.ric_instance_id}].indication_task =
      launch_async<e2_indication_procedure>(
          tx_pdu_notifier,
          ev_mng,
          subscriptions[{ric_request_id.ric_requestor_id, ric_request_id.ric_instance_id}].subscription_info,
          logger);
}

void e2_subscription_manager_impl::stop_subscription(const asn1::e2ap::ric_request_id_s&         ric_request_id,
                                                     e2_event_manager&                           ev_mng,
                                                     const asn1::e2ap::ric_sub_delete_request_s& msg)
{
  if (subscriptions.count({ric_request_id.ric_requestor_id, ric_request_id.ric_instance_id})) {
    ev_mng.sub_del_reqs[{ric_request_id.ric_requestor_id, ric_request_id.ric_instance_id}]->set(msg);
    subscriptions[{ric_request_id.ric_requestor_id, ric_request_id.ric_instance_id}].indication_task.await_ready();
    subscriptions.erase({ric_request_id.ric_requestor_id, ric_request_id.ric_instance_id});
  } else {
    logger.error("RIC instance ID not found");
  }
}
bool e2_subscription_manager_impl::action_supported(const ric_action_to_be_setup_item_s& action,
                                                    uint16_t                             ran_func_id,
                                                    const asn1::e2ap::ric_request_id_s&  ric_request_id)
{
  e2sm_interface* e2sm = e2sm_mngr.get_e2sm_interface(ran_func_id);
  if (e2sm == nullptr) {
    logger.error("Failed to get E2SM interface, RAN function {} not in allowed list", ran_func_id);
    return false;
  }

  auto action_def_buf = action.ric_action_definition.deep_copy();
  if (not action_def_buf.has_value()) {
    logger.warning("Failed to deep copy a byte_buffer");
    return false;
  }

  if (e2sm->action_supported(action)) {
    subscriptions[{ric_request_id.ric_requestor_id, ric_request_id.ric_instance_id}]
        .subscription_info.action_list.push_back(
            {std::move(action_def_buf.value()), action.ric_action_id, action.ric_action_type});
    return true;
  }

  logger.error("Action not supported {}", action.ric_action_id);
  return false;
}

void e2_subscription_manager_impl::get_subscription_result(uint16_t                              ran_func_id,
                                                           e2_subscribe_reponse_message&         outcome,
                                                           e2_subscription_t&                    subscription,
                                                           const ric_actions_to_be_setup_list_l& actions)
{
  outcome.success                     = false;
  outcome.request_id.ric_requestor_id = subscription.subscription_info.request_id.ric_requestor_id;
  outcome.request_id.ric_instance_id  = subscription.subscription_info.request_id.ric_instance_id;
  for (unsigned i = 0, e = actions.size(); i != e; ++i) {
    auto& action = actions[i].value().ric_action_to_be_setup_item();
    if (action_supported(action, ran_func_id, outcome.request_id)) {
      outcome.success = true;
      outcome.admitted_list.resize(outcome.admitted_list.size() + 1);
      outcome.admitted_list.back().value().ric_action_admitted_item().ric_action_id = action.ric_action_id;
    } else {
      outcome.not_admitted_list.resize(outcome.not_admitted_list.size() + 1);
      outcome.not_admitted_list.back().value().ric_action_not_admitted_item().ric_action_id = action.ric_action_id;
    }
  }
}

void e2_subscription_manager_impl::add_e2sm_service(std::string oid, std::unique_ptr<e2sm_interface> e2sm_iface)
{
  e2sm_iface_list.emplace(oid, std::move(e2sm_iface));
}

e2sm_interface* e2_subscription_manager_impl::get_e2sm_interface(std::string oid)
{
  auto it = e2sm_iface_list.find(oid);
  if (it == e2sm_iface_list.end()) {
    logger.error("OID not supported");
    return nullptr;
  }
  return &(*(it->second));
}

void e2_subscription_manager_impl::add_ran_function_oid(uint16_t ran_func_id, std::string oid)
{
  if (e2sm_mngr.get_e2sm_interface(oid) != nullptr) {
    supported_ran_functions.emplace(ran_func_id, oid);
  } else {
    logger.error("OID not supported");
  }
}
