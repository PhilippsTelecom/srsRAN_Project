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

#include "f1ap_ue_context.h"
#include "srsran/adt/slotted_array.h"
#include "srsran/f1ap/du/f1c_bearer.h"
#include "srsran/f1ap/du/f1c_rx_sdu_notifier.h"
#include "srsran/f1ap/f1ap_message_notifier.h"
#include "srsran/f1u/du/f1u_bearer.h"
#include "srsran/f1u/du/f1u_rx_sdu_notifier.h"
#include "srsran/f1u/du/f1u_tx_pdu_notifier.h"
#include "srsran/ran/lcid.h"
#include "srsran/ran/nr_cgi.h"
#include "srsran/support/executors/task_executor.h"

namespace srsran {
namespace srs_du {

class f1ap_event_manager;
class f1ap_du_configurator;

class ue_bearer_manager
{
public:
  ue_bearer_manager(f1ap_ue_context&       ue_ctx_,
                    f1ap_message_notifier& f1ap_notif_,
                    f1ap_du_configurator&  du_configurator_,
                    task_executor&         ctrl_exec_,
                    task_executor&         ue_exec_) :
    ue_ctx(ue_ctx_),
    f1ap_notifier(f1ap_notif_),
    du_configurator(du_configurator_),
    ctrl_exec(ctrl_exec_),
    ue_exec(ue_exec_)
  {
  }

  void add_srb0_f1c_bearer(f1c_rx_sdu_notifier&       f1c_rx_sdu_notif,
                           const nr_cell_global_id_t& pcell_cgi,
                           const byte_buffer&         du_cu_rrc_container,
                           f1ap_event_manager&        ev_mng);

  void add_f1c_bearer(srb_id_t srb_id, f1c_rx_sdu_notifier& rx_sdu_notif);

  f1c_bearer* find_srb(srb_id_t srb_id)
  {
    return f1c_bearers.contains(srb_id_to_uint(srb_id)) ? f1c_bearers[srb_id_to_uint(srb_id)].get() : nullptr;
  }
  const f1c_bearer* find_srb(srb_id_t srb_id) const
  {
    return f1c_bearers.contains(srb_id_to_uint(srb_id)) ? f1c_bearers[srb_id_to_uint(srb_id)].get() : nullptr;
  }

private:
  f1ap_ue_context&       ue_ctx;
  f1ap_message_notifier& f1ap_notifier;
  f1ap_du_configurator&  du_configurator;
  task_executor&         ctrl_exec;
  task_executor&         ue_exec;

  slotted_array<std::unique_ptr<f1c_bearer>, MAX_NOF_SRBS> f1c_bearers;
};

} // namespace srs_du
} // namespace srsran
