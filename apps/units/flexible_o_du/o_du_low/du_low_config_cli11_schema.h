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

#include "srsran/ran/duplex_mode.h"
#include "CLI/CLI11.hpp"

namespace srsran {

struct du_low_unit_config;

/// Configures the given CLI11 application with the DU low configuration schema.
void configure_cli11_with_du_low_config_schema(CLI::App& app, du_low_unit_config& parsed_cfg);

/// Auto derive DU low parameters after the parsing.
void autoderive_du_low_parameters_after_parsing(CLI::App&           app,
                                                du_low_unit_config& parsed_cfg,
                                                duplex_mode         mode,
                                                bool                is_blocking_mode_enabled,
                                                unsigned            nof_cells);

} // namespace srsran
