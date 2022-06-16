/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#ifndef SRSGNB_LIB_FAPI_ADAPTOR_PHY_FAPI_TO_PHY_TRANSLATOR_H
#define SRSGNB_LIB_FAPI_ADAPTOR_PHY_FAPI_TO_PHY_TRANSLATOR_H

#include "srsgnb/fapi/slot_message_gateway.h"
#include "srsgnb/ran/slot_point.h"
#include <mutex>

namespace srsgnb {

class downlink_processor;
class downlink_processor_pool;
class resource_grid_pool;

namespace fapi_adaptor {

/// \brief This class receives FAPI slot based messages and translates them into PHY specific data types, commanding the
/// upper PHY to process them.
///
/// Translating and processing the FAPI slot based messages involves converting the data structures, checking for
/// message validity at slot level and selecting and reserving resources in the PHY to process the messages.
///
/// \note The translator assumes that the contents of the incoming FAPI slot message are valid, ie: it has been
/// previously validated using the provided FAPI validators.
/// \note This class has been designed to be thread safe to allow calling the \c set_handle() method and message
/// handlers from different threads.
class fapi_to_phy_translator : public fapi::slot_message_gateway
{
  /// RAII style class which is meant to have a lifetime of a single slot. It executes the preparation and closing
  /// procedures required by the upper PHY within a slot.
  ///
  /// \note The lifetime of this object is meant to be a single slot point.
  class slot_based_upper_phy_controller
  {
    slot_point                                 slot;
    std::reference_wrapper<downlink_processor> dl_processor;

  public:
    slot_based_upper_phy_controller();

    slot_based_upper_phy_controller(downlink_processor_pool& dl_processor_pool,
                                    resource_grid_pool&      rg_pool,
                                    slot_point               slot,
                                    unsigned                 sector_id);

    slot_based_upper_phy_controller(slot_based_upper_phy_controller&& other) = delete;

    slot_based_upper_phy_controller& operator=(slot_based_upper_phy_controller&& other);

    ~slot_based_upper_phy_controller();

    downlink_processor*       operator->() { return &dl_processor.get(); }
    const downlink_processor* operator->() const { return &dl_processor.get(); }
  };

public:
  fapi_to_phy_translator(unsigned sector_id, downlink_processor_pool& dl_processor_pool, resource_grid_pool& rg_pool) :
    sector_id(sector_id), dl_processor_pool(dl_processor_pool), rg_pool(rg_pool)
  {
  }

  // See interface for documentation.
  void dl_tti_request(const fapi::dl_tti_request_message& msg) override;

  // See interface for documentation.
  void ul_tti_request(const fapi::ul_tti_request_message& msg) override;

  // See interface for documentation.
  void ul_dci_request(const fapi::ul_dci_request_message& msg) override;

  // See interface for documentation.
  void tx_data_request(const fapi::tx_data_request_message& msg) override;

  /// \brief Handles a new slot.
  ///
  /// Handling a new slot consists of the following steps:
  /// - Finishing processing the PDUs from the previous slot.
  /// - Updating the current slot.
  /// - Grabbing a resource grid and a downlink processor.
  /// - Configuring the downlink processor with the new resource grid.
  ///
  /// \param slot Identifies the new slot.
  /// \note This method may be called from a different thread compared to the rest of methods.
  void handle_new_slot(slot_point slot);

private:
  const unsigned           sector_id;
  downlink_processor_pool& dl_processor_pool;
  resource_grid_pool&      rg_pool;

  slot_based_upper_phy_controller current_slot_controller;
  // Protects current_slot_controller.
  //: TODO: make this lock free.
  std::mutex mutex;
};

} // namespace fapi_adaptor
} // namespace srsgnb

#endif // SRSGNB_LIB_FAPI_ADAPTOR_PHY_FAPI_TO_PHY_TRANSLATOR_H
