/**
 * Copyright 2026-2026 Fraunhofer Institute for Applied Information Technology
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
 * @file GilState.h
 * @brief portable tests for the GIL ownership of the calling thread
 *
 * @package iec104-python
 * @namespace module
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_MODULE_GILSTATE_H
#define C104_MODULE_GILSTATE_H

#include <pybind11/detail/internals.h>

#include "types.h"

namespace Module {

/**
 * @brief Test if the calling thread is currently attached to a python thread
 * state.
 *
 * lib60870 calls into python from threads it created itself, so module code
 * has to decide per call whether the GIL may be released or has to be
 * acquired. PyGILState_Check() is not usable for that decision: CPython
 * disables its bookkeeping permanently as soon as a second interpreter is
 * created (Py_NewInterpreter() -> gilstate.check_enabled = 0) and reports
 * "GIL held" for every thread afterwards, including foreign threads that
 * never saw a thread state. Acting on that answer aborts the process, e.g.
 * py::gil_scoped_release() asserts on the very same PyGILState_Check() and
 * PyEval_SaveThread() terminates on a missing thread state.
 *
 * The thread state pointer is what both use internally, so test it directly:
 * it is NULL exactly when this thread is not attached, which on a GIL build
 * means the GIL is not held (PyEval_SaveThread() detaches the thread state).
 * pybind11 selects the supported accessor per interpreter build:
 * PyThreadState_GetUnchecked() on 3.13+, the _PyThreadState_UncheckedGet()
 * alias on 3.9 to 3.12, and PyThreadState_GET() on PyPy and GraalVM. All of
 * them return NULL instead of aborting on an unattached thread.
 *
 * On a free-threaded build (3.13t+) a thread keeps its thread state without
 * holding the GIL, so this answers only "the interpreter knows this thread":
 * that is sufficient, because there the GIL acquisition and release helpers
 * are no-ops and the callers only need to avoid them on foreign threads.
 *
 * @note This is a point-in-time observation of the calling thread, not a
 * guarantee that stays valid after another thread started finalization.
 * @note It says nothing about subinterpreter support: a thread attached to a
 * different interpreter also reports true. This module does not support being
 * imported into more than one interpreter.
 *
 * @return true if the calling thread has a python thread state
 */
inline bool hasAttachedThreadState() noexcept {
  return py::detail::get_thread_state_unchecked() != nullptr;
}

/**
 * @brief Test if the interpreter is shutting down.
 *
 * Caller must hold the GIL (or have restarted finalization protection
 * otherwise), Py_IsFinalizing() is not safe on a thread without a thread
 * state. On a free-threaded build this only reports the global flag.
 *
 * @return true if interpreter finalization has started
 */
inline bool isFinalizing() {
#if PY_VERSION_HEX >= 0x030D0000
  return Py_IsFinalizing() != 0;
#else
  return _Py_IsFinalizing() != 0;
#endif
}

} // namespace Module

#endif // C104_MODULE_GILSTATE_H
