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

#include "Selection.h"

using namespace Remote;

SelectionManager::SelectionManager(uint_fast16_t select_timeout_ms)
    : selectTimeout_ms(select_timeout_ms) {}

void SelectionManager::cleanup() {
  if (activeSelections.load() == 0)
    return;

  auto now = std::chrono::steady_clock::now();
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  selectionVector.erase(
      std::remove_if(selectionVector.begin(), selectionVector.end(),
                     [this, now](const Selection &s) {
                       return (now - s.created) < selectTimeout_ms;
                     }),
      selectionVector.end());
  activeSelections.store(selectionVector.size());
}

bool SelectionManager::add(const Remote::Selection &selection) {
  auto now = std::chrono::steady_clock::now();

  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it = std::find_if(selectionVector.begin(), selectionVector.end(),
                         [selection](const Selection &existing) {
                           return existing.ca == selection.ca &&
                                  existing.ioa == selection.ioa;
                         });

  // found, but timed out => remove
  if (it != selectionVector.end() && (now - it->created) < selectTimeout_ms) {
    it = selectionVector.erase(it);
  }

  // selection NOT found
  if (it == selectionVector.end()) {
    selectionVector.push_back(selection);
    activeSelections.store(activeSelections.load() + 1);
    return true;
  }

  // extend existing selection
  if (it->oa == selection.oa) {
    it->created = now;
  }

  // already selected by someone else
  return false;
}

void SelectionManager::replace(const uint8_t oa, const uint16_t ca,
                               const uint32_t ioa) {
  auto now = std::chrono::steady_clock::now();

  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it = std::find_if(selectionVector.begin(), selectionVector.end(),
                         [ca, ioa](const Selection &existing) {
                           return existing.ca == ca && existing.ioa == ioa;
                         });

  // found, but timed out => remove
  if (it == selectionVector.end()) {
    selectionVector.push_back({oa, ca, ioa, now});
    activeSelections.store(activeSelections.load() + 1);
  } else {
    it->oa = oa;
    it->created = now;
  }
}

void SelectionManager::remove(uint16_t ca, uint32_t ioa) {
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  selectionVector.erase(std::remove_if(selectionVector.begin(),
                                       selectionVector.end(),
                                       [ca, ioa](const Selection &s) {
                                         return s.ca == ca && s.ioa == ioa;
                                       }),
                        selectionVector.end());
  activeSelections.store(selectionVector.size());
}

bool SelectionManager::exists(const Remote::Selection &selection) {
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it = std::find_if(selectionVector.begin(), selectionVector.end(),
                         [selection](const Selection &existing) {
                           return existing.ca == selection.ca &&
                                  existing.ioa == selection.ioa;
                         });

  // found, but timed out => remove
  if (it != selectionVector.end() &&
      (selection.created - it->created) < selectTimeout_ms) {
    it = selectionVector.erase(it);
  }

  // selection NOT found
  if (it == selectionVector.end()) {
    return false;
  }

  // found valid selection
  if (it->oa != 0 && it->oa == selection.oa) {
    return true;
  }

  // already selected by someone else
  return false;
}

std::optional<const Selection> SelectionManager::get(const uint16_t ca,
                                                     const uint32_t ioa) const {
  auto now = std::chrono::steady_clock::now();
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it = std::find_if(
      selectionVector.begin(), selectionVector.end(),
      [ca, ioa](const Selection &s) { return s.ca == ca && s.ioa == ioa; });

  // selection NOT found
  if (it != selectionVector.end() && (now - it->created) < selectTimeout_ms) {
    return *it;
  }
  return std::nullopt;
}
