/**
 * Copyright 2023-2025 Fraunhofer Institute for Applied Information Technology
 * FIT
 *
 * This file is part of iec104-python.
 * iec104-python is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * iec104-python is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with iec104-python. If not, see <https://www.gnu.org/licenses/>.
 *
 *  See LICENSE file for the complete license text.
 *
 *
 * @file Selection.cpp
 * @brief manage point selection state
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_SELECTION_H
#define C104_REMOTE_SELECTION_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

#include "module/GilAwareMutex.h"

namespace Remote {

/**
 * @brief Represents a selection within the server for select-and-execute
 * patterns.
 *
 * The `Selection` structure is used to maintain information related to a
 * specific select-and-execute process. This includes details such as the select
 * ASDU, originating address (OA), common address (CA), information object
 * address (IOA), and the master connection that initiated the selection.
 * Additionally, it stores the timestamp indicating when the selection was
 * created to test for timeouts.
 */
struct Selection {
  uint8_t oa;
  uint16_t ca;
  uint32_t ioa;
  std::chrono::steady_clock::time_point created;
};

class SelectionManager {
private:
  /// @brief selection init timestamp, to test against timeout
  const std::chrono::milliseconds selectTimeout_ms{10000};

  /// @brief MUTEX Lock to access selectionVEcotr
  mutable Module::GilAwareMutex selection_mutex{"Server::selection_mutex"};

  /// @brief vector of all selections
  std::vector<Selection> selectionVector{};

  /// @brief number of active selections
  std::atomic_uint_fast8_t activeSelections{0};

public:
  SelectionManager(uint_fast16_t select_timeout_ms = 10000);

  /**
   * @brief Cleans up expired selections within the server.
   *
   * The `cleanup` method removes outdated selections from the
   * server's selection list. This process iterates through the current
   * selections to determine if their lifetime has exceeded the configured
   * selection timeout. Removed selections will trigger an onUnselect operation.
   */
  void cleanup();

  bool add(const Remote::Selection &selection);

  void replace(uint8_t oa, uint16_t ca, uint32_t ioa);

  /**
   * @brief Removes a selection associated with the specified common address
   * (CA) and information object address (IOA).
   *
   * This method removes a selection from the selection vector based on the
   * provided CA and IOA. If a matching selection is found, it is unselected and
   * removed. Removed selections will trigger an onUnselect operation. This
   * method is used to delay the activation termination message after the actual
   * command response.
   *
   * @param ca The common address (CA) of the selection to be removed.
   * @param ioa The information object address (IOA) of the selection to be
   * removed.
   */
  void remove(uint16_t ca, uint32_t ioa);

  bool exists(const Remote::Selection &selection);

  /**
   * @brief Retrieves the selector associated with a given common address (ca)
   * and information object address (ioa) from the selection vector.
   *
   * This function searches for a matching selector in the selection vector
   * based on the provided parameters. If a selection is found
   * and the time elapsed since its creation does not exceed
   * the configured selection timeout, the function returns the selector;
   * otherwise, it returns an empty optional.
   *
   * @param ca The common address to search for.
   * @param ioa The information object address to search for.
   * @return An optional containing the selector if found and valid;
   *         otherwise, an empty optional.
   */
  std::optional<const Selection> get(uint16_t ca, uint32_t ioa) const;
};

} // namespace Remote

#endif // C104_REMOTE_SELECTION_H
