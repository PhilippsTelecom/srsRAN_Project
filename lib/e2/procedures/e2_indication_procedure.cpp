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

#include "e2_indication_procedure.h"
#include "srsran/asn1/e2ap/e2ap.h"
#include "srsran/support/async/async_timer.h"

using namespace srsran;
using namespace asn1::e2ap;

e2_indication_procedure::e2_indication_procedure(e2_message_notifier&    notif_,
                                                 e2_event_manager&       ev_mng_,
                                                 e2_subscription_info_t& subscription_info_,
                                                 srslog::basic_logger&   logger_) :
  notifier(notif_), ev_mng(ev_mng_), subscription(subscription_info_), logger(logger_)
{
}

void e2_indication_procedure::operator()(coro_context<eager_async_task<void>>& ctx)
{
  CORO_BEGIN(ctx);
  while (running) {
    if (!ev_mng.sub_del_reqs.count(
            {subscription.request_id.ric_requestor_id, subscription.request_id.ric_instance_id})) {
      logger.error("No subscription delete request found for RIC request ID (Requestor ID={}, Instance ID={})",
                   subscription.request_id.ric_requestor_id,
                   subscription.request_id.ric_instance_id);
      break;
    }
    transaction_sink.subscribe_to(
        *ev_mng.sub_del_reqs[{subscription.request_id.ric_requestor_id, subscription.request_id.ric_instance_id}].get(),
        (std::chrono::milliseconds)subscription.report_period);
    CORO_AWAIT(transaction_sink);
    if (!transaction_sink.timeout_expired()) {
      logger.info("Subscription deleted");
      running = false;
    }
    for (const auto& action : subscription.action_list) {
      e2_indication_message e2_ind = {};
      //      e2_ind.indication->ric_ind_msg.crit                = asn1::crit_opts::reject;
      e2_ind.request_id                                  = subscription.request_id;
      e2_ind.indication->ran_function_id                 = subscription.ran_function_id;
      e2_ind.indication->ric_action_id                   = action.ric_action_id;
      e2_ind.indication->ric_call_process_id_present     = false;
      e2_ind.indication->ric_ind_sn_present              = false;
      e2_ind.indication->ric_request_id.ric_instance_id  = subscription.request_id.ric_instance_id;
      e2_ind.indication->ric_request_id.ric_requestor_id = subscription.request_id.ric_requestor_id;

      byte_buffer ind_msg_bytes;
      byte_buffer ind_hdr_bytes;
      switch (action.ric_action_type) {
        case ric_action_type_e::ric_action_type_opts::report:
          e2_ind.indication->ric_ind_type.value = ric_ind_type_e::ric_ind_type_opts::report;
          // Trigger measurement collection.
          action.report_service->collect_measurements();
          if (action.report_service->is_ind_msg_ready()) {
            // Get RIC indication msg content.
            ind_msg_bytes = action.report_service->get_indication_message();
            ind_hdr_bytes = action.report_service->get_indication_header();
          } else {
            continue;
          }
          break;
        case ric_action_type_e::ric_action_type_opts::insert:
          e2_ind.indication->ric_ind_type.value = ric_ind_type_e::ric_ind_type_opts::insert;
          break;
        default:
          logger.error("Unknown action type");
          break;
      }

      // Put RIC indication content into message.
      if (!e2_ind.indication->ric_ind_msg.resize(ind_msg_bytes.length())) {
        logger.error("Unable to resize byte_buffer, dropping indication");
        continue;
      }
      std::copy(ind_msg_bytes.begin(), ind_msg_bytes.end(), e2_ind.indication->ric_ind_msg.begin()); // Copy ind_msg_bytes to ric_ind_msg
      if (!e2_ind.indication->ric_ind_hdr.resize(ind_hdr_bytes.length())) {
        logger.error("Unable to resize byte_buffer, dropping indication");
        continue;
      }
      std::copy(ind_hdr_bytes.begin(), ind_hdr_bytes.end(), e2_ind.indication->ric_ind_hdr.begin()); // Copy ind_hdr_bytes to ric_ind_hdr
      logger.info("Sending E2 indication");
      send_e2_indication(e2_ind);
    }
  }
  CORO_RETURN();
}

void e2_indication_procedure::send_e2_indication(e2_indication_message& e2_ind)
{
  e2_message e2_msg;
  e2_msg.pdu.set_init_msg();
  e2_msg.pdu.init_msg().load_info_obj(ASN1_E2AP_ID_RIC_IND);
  e2_msg.pdu.init_msg().value.ric_ind() = e2_ind.indication;
  e2_msg.pdu.init_msg().crit            = asn1::crit_opts::ignore;
  notifier.on_new_message(e2_msg);
}
