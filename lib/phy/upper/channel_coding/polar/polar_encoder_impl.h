/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsgnb/phy/upper/channel_coding/polar/polar_encoder.h"
#include "srsgnb/srsvec/aligned_vec.h"

namespace srsgnb {

class polar_encoder_impl : public polar_encoder
{
public:
  // See interface for documentation.
  void encode(span<uint8_t> output, span<const uint8_t> input, unsigned code_size_log) override;
};

} // namespace srsgnb
