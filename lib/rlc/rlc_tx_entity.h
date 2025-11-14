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

#include "rlc_bearer_logger.h"
#include "rlc_metrics_aggregator.h"
#include "rlc_tx_metrics_container.h"
#include "srsran/pcap/rlc_pcap.h"
#include "srsran/rlc/rlc_tx.h"
// Associating
#include <cstdint>
#include <unordered_map>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <filesystem>

namespace srsran {

  struct rc_to_proba {
    int current_id  = -1; // Current RC_ID (cf RTP Payload)
    int nb_elements = 0;  // Nb SN having same id 
    int sum_probas  = 0;  // Sum of DU probas
    std::unordered_map<int,double> associations;  // Associates RC_ID to probability
  };

/// Base class used for transmitting RLC bearers.
/// It provides interfaces for the RLC bearers, for both higher layers and lower layers
/// It also stores interfaces for the higher layers, both for the user-plane
/// and the control plane.
class rlc_tx_entity : public rlc_tx_upper_layer_data_interface,
                      public rlc_tx_lower_layer_interface,
                      public rlc_tx_metrics
{
protected:
  rlc_tx_entity(gnb_du_id_t                          gnb_du_id,
                du_ue_index_t                        ue_index,
                rb_id_t                              rb_id_,
                rlc_tx_upper_layer_data_notifier&    upper_dn_,
                rlc_tx_upper_layer_control_notifier& upper_cn_,
                rlc_tx_lower_layer_notifier&         lower_dn_,
                rlc_metrics_aggregator&              metrics_agg_,
                rlc_pcap&                            pcap_,
                task_executor&                       pcell_executor_,
                task_executor&                       ue_executor_,
                timer_manager&                       timers) :
    logger("RLC", {gnb_du_id, ue_index, rb_id_, "DL"}),
    metrics_high(metrics_agg_.get_metrics_period().count()),
    metrics_low(metrics_agg_.get_metrics_period().count()),
    ue_index_(ue_index),
    rb_id(rb_id_),
    upper_dn(upper_dn_),
    upper_cn(upper_cn_),
    lower_dn(lower_dn_),
    pcap(pcap_),
    pcell_executor{pcell_executor_},
    ue_executor{ue_executor_},
    pcell_timer_factory{timers, pcell_executor},
    ue_timer_factory{timers, ue_executor},
    high_metrics_timer(pcell_timer_factory.create_timer()),
    low_metrics_timer(ue_timer_factory.create_timer()),
    metrics_agg(metrics_agg_)
  {
    if (metrics_agg.get_metrics_period().count()) {
      high_metrics_timer.set(std::chrono::milliseconds(metrics_agg.get_metrics_period().count()),
                             [this](timer_id_t tid) {
                               metrics_agg.push_tx_high_metrics(metrics_high.get_and_reset_metrics());
                               high_metrics_timer.run();
                             });
      low_metrics_timer.set(std::chrono::milliseconds(metrics_agg.get_metrics_period().count()),
                            [this](timer_id_t tid) {
                              metrics_agg.push_tx_low_metrics(metrics_low.get_and_reset_metrics());
                              low_metrics_timer.run();
                            });

      high_metrics_timer.run();
      low_metrics_timer.run();
    }
  }

  rlc_bearer_logger                    logger;
  rlc_tx_metrics_high_container        metrics_high;
  rlc_tx_metrics_low_container         metrics_low;
  du_ue_index_t                        ue_index_;
  rb_id_t                              rb_id;
  rlc_tx_upper_layer_data_notifier&    upper_dn;
  rlc_tx_upper_layer_control_notifier& upper_cn;
  rlc_tx_lower_layer_notifier&         lower_dn;
  rlc_pcap&                            pcap;
  task_executor&                       pcell_executor;
  task_executor&                       ue_executor;
  timer_factory                        pcell_timer_factory;
  timer_factory                        ue_timer_factory;

  // Associates probability to certain RC ID
  rc_to_proba                           proba_assoc;

  unique_timer high_metrics_timer;
  unique_timer low_metrics_timer;

private:
  rlc_metrics_aggregator& metrics_agg;

  /// \brief Receives a modified IP header (ECN-CE marked)
  /// Computes the checksum of this header
  /// Returns the modified checksum made of 2 bytes (checksum on 2 bytes)
  ///
  /// \param data pointer towards the IP header
  /// \param result table of 2 bytes, checksum put inside
 static void compute_checksum(uint8_t* data, uint8_t result[2]){
   uint32_t sum=0; 
   // data equals to 4*5 bytes = 20 bytes
   for (int i=0 ; i<10 ; i++){
     // Concatenation
     uint16_t word = (static_cast<uint16_t>(*(data+i*2)) << 8) | *(data+i*2+1);
     sum+=word;
     // Check if overflow
     if (sum > 0xFFFF) {
       sum = (sum & 0xFFFF) + (sum >> 16);
      }
    }
      
  uint16_t chksum;
  chksum = ~sum;
  // chksum as 2 distinct bytes
  result[0]=(chksum >> 8) & 0xFF; // two strong bytes
  result[1]=chksum & 0xFF; // two weak bytes
 }

public:
  /// \brief Stops all internal timers.
  ///
  /// This function is inteded to be called upon removal of the bearer before destroying it.
  /// It stops all timers with handlers that may delegate tasks to another executor that could face a deleted object at
  /// a later execution time.
  /// Before this function is called, the adjacent layers should already be disconnected so that no timer is restarted.
  ///
  /// Note: This function shall only be called from ue_executor.
  virtual void stop() = 0;

  rlc_tx_metrics get_metrics()
  {
    rlc_tx_metrics m;
    m.tx_high = metrics_high.get_hi_metrics();
    m.tx_low  = metrics_low.get_low_metrics();
    return m;
  }

  /// \brief Marks a packet (ECN-CE)
  /// \param sdu data coming from the above layer (PDCP)
  /// \param pdcp_header size of the PDCP header
  static void mark_l4s_packet(rlc_sdu sdu, unsigned pdcp_header){
    // Extracts IP Header (5 lines = 5*4 bytes)
    uint8_t* ip_header = sdu.buf.get_payload_(pdcp_header + 0,4*5);
    if (ip_header != nullptr){
      // Set ECN-CE Flag (2nd byte)
      *(ip_header+1) |= 0b00000011; 
      sdu.buf.set_payload_(pdcp_header + 1,*(ip_header+1));
            
      // Sets old checksum to 0 (11th & 12th byte)
      *(ip_header + 4*2 + 2) &= 0b00000000;
      *(ip_header + 4*2 + 3) &= 0b00000000;
            
      // Compute new Checksum
      uint8_t checksum[2];
      compute_checksum(ip_header,checksum);

      // Modify checksum
      sdu.buf.set_payload_(pdcp_header + 4*2 + 2,*checksum);
      sdu.buf.set_payload_(pdcp_header + 4*2 + 3,*(checksum+1));          
    }
    // Free header
    free(ip_header);
  }

  /// \brief Used to retrieve the Radio-Control ID in the RTP Payload
  /// Only used for training the model (the goal is to associate the E2SM-RC probability to the DU probability)
  /// \param sdu_buf data coming from the above layer (PDCP)
  /// \param pdcp_header size of the PDCP header
  /// \param ip_eader size of the IP header
  static int get_rc_id(byte_buffer& sdu_buf,int pdcp_header, int ip_header){
    int rc_id = -1;

    // (I) Ensure UDP is used by looking IP Header
    uint8_t* protocol = sdu_buf.get_payload_(pdcp_header+4*2+1,1);
    if(protocol != nullptr){ 
      if(*protocol == 17){
      // (II) Ensure correct RTP version and not extension by looking RTP Header
      uint8_t* rtp_start  = sdu_buf.get_payload_(pdcp_header+4*(ip_header + 2), 1); // +2 (64 bits UDP Header)
        if(rtp_start != nullptr){
          int rtp_version     = (*rtp_start >> 6) & 0x03;
          bool extension      = (*rtp_start >> 4) & 0x01;
          int nb_csrc         = *rtp_start & 0x0f;
          free(rtp_start);

          if(rtp_version == 2 && !extension){
            uint8_t* read_value = sdu_buf.get_payload_(pdcp_header+4*(ip_header + 2 + 3 + nb_csrc),4);
            if(read_value != nullptr){
              rc_id = (read_value[0] << 24) | (read_value[1] << 16) | (read_value[2] << 8) | read_value[3];
              free(read_value);
            }
          }
        }
      }
      free(protocol);
    }
    return rc_id;
  }

  /// \brief Used to save dictionary <rc_id, proba> in a CSV file (folder RC_Probas/)
  /// Only used for training the model (the goal is to associate the E2SM-RC probability to the DU probability)
  void save_rc_probas(){
    // Directory 
    const char* directory = "./RC_Probas/";
    struct stat sb;
    if(! (stat(directory, &sb)==0)){
      std::filesystem::create_directory(directory);
    }
    // File Name: UE_DRB.csv
    std::string file_name; 
    uint8_t rb = drb_id_to_uint(rb_id.get_drb_id());
    file_name = std::string(directory)+"UE"+std::to_string(ue_index_)+"_DRB"+std::to_string(rb)+".csv";
    // Open File
    std::fstream fout;
    fout.open(file_name, std::ios::out);
    // Write File
    for (const auto& [key, value] : proba_assoc.associations){
      fout << key << "," << value << "\n";
    }
    // Close File
    fout.close();
  }
};

} // namespace srsran
