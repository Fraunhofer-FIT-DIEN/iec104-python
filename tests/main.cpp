/**
 * Copyright 2020-2026 Fraunhofer Institute for Applied Information Technology
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
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <catch2/catch_session.hpp>
#include <iostream>
#include <pybind11/embed.h> // everything needed for embedding
namespace py = pybind11;

int main(int argc, char *argv[]) {
#ifdef _WIN32
  system("chcp 65001 > nul");
#endif

  py::scoped_interpreter guard{};

  // Load the c104 module so its C++ types (e.g. the IEC60870_5_TypeID enum)
  // are registered in this embedded interpreter, mirroring a normal Python
  // import. This lets the C++ tests exercise the py::dict-based helpers (such
  // as Remote::rawMessageDictionaryFormatter) with the same type registration
  // as production.
  try {
    py::module_::import("c104");
  } catch (const std::exception &e) {
    std::cerr << "[c104_tests] warning: could not import c104 module: "
              << e.what()
              << " (py::dict helpers relying on registered enum types may not "
                 "be fully exercisable)\n";
  }

  int result = Catch::Session().run(argc, argv);

  return result;
}
