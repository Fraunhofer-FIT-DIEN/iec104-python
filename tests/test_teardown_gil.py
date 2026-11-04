#!/usr/bin/env python3
"""
Copyright 2026 Fraunhofer Institute for Applied Information Technology FIT

This file is part of iec104-python.
... (GPL header)

Subprocess regression tests for the teardown deadlock described in SECURITY.md
M10: Server::~Server and Client::~Client joined their run thread while still
holding the GIL. A run thread that was inside a python callback (Point.on_timer)
therefore never came back from ScopedGilAcquire, and the join - which does not
release the GIL - blocked forever. Dropping the last python reference without
an explicit stop() was enough to hang the interpreter.

Each test runs its scenario in a child interpreter, drops the last python
reference to a started Server or Client while a callback is in flight, and
requires the child to exit on its own. A deadlock shows up as
subprocess.TimeoutExpired in the parent, because the child is killed instead of
exiting.

Run this suite like the other ones, with the installed c104 package and from a
directory other than the repo root:
    cd /tmp && python3 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_teardown_gil.py' -v
"""

import shutil
import subprocess
import sys
import tempfile
import unittest


CHILD_TIMEOUT_S = 20.0

SERVER_PORT = 26411
CLIENT_PORT = 26412


_PREAMBLE = """
import sys
import threading

import c104

# The GIL is only handed over on request, and that request is made once the
# switch interval elapsed. A long interval keeps the teardown thread on the GIL
# from the callback handshake up to the destructor, so the run thread cannot
# finish its callback in between and hide the defect.
sys.setswitchinterval(10.0)

callback_entered = threading.Event()
callback_release = threading.Event()


def on_timer(point: c104.Point) -> None:
    callback_entered.set()
    callback_release.wait()
"""


_SERVER_CHILD = (
    _PREAMBLE
    + """
port = {port}
server = c104.Server(ip="127.0.0.1", port=port, tick_rate_ms=50)
station = server.add_station(common_address=11)
point = station.add_point(io_address=1, type=c104.Type.M_SP_NA_1)
point.on_timer(callable=on_timer, interval_ms=50)

# a server only runs point timers while a connection is active
client = c104.Client(tick_rate_ms=50)
connection = client.add_connection(ip="127.0.0.1", port=port, init=c104.Init.NONE)

server.start()
client.start()

assert callback_entered.wait(10.0), "server on_timer never fired"

# the run thread is now blocked inside the python callback: the destructor has
# to join it. Dropping the reference must not need an explicit stop().
callback_release.set()
del server
print("SERVER_DROP_OK", flush=True)

client.stop()
print("SERVER_CHILD_OK", flush=True)
""".format(port=SERVER_PORT)
)


_CLIENT_CHILD = (
    _PREAMBLE
    + """
port = {port}
server = c104.Server(ip="127.0.0.1", port=port, tick_rate_ms=50)
server_station = server.add_station(common_address=11)

client = c104.Client(tick_rate_ms=50)
connection = client.add_connection(ip="127.0.0.1", port=port, init=c104.Init.NONE)
station = connection.add_station(common_address=11)
point = station.add_point(io_address=1, type=c104.Type.M_SP_NA_1)
point.on_timer(callable=on_timer, interval_ms=50)

server.start()
client.start()

assert callback_entered.wait(10.0), "client on_timer never fired"

# same as above, but on the client side: its run thread sits in the callback
callback_release.set()
del client
print("CLIENT_DROP_OK", flush=True)

# this case is about the client, the loopback server is stopped explicitly
del connection, point, station
server.stop()
print("CLIENT_CHILD_OK", flush=True)
""".format(port=CLIENT_PORT)
)


class SubprocessTeardownTestCase(unittest.TestCase):
    def run_child(self, script):
        workdir = tempfile.mkdtemp(prefix="c104-teardown-gil-")
        try:
            return subprocess.run(
                [sys.executable, "-c", script],
                cwd=workdir,
                capture_output=True,
                text=True,
                timeout=CHILD_TIMEOUT_S,
            )
        finally:
            shutil.rmtree(workdir, ignore_errors=True)

    def assert_child_dropped(self, script, marker):
        try:
            result = self.run_child(script)
        except subprocess.TimeoutExpired as e:
            self.fail(
                "child did not exit within {0:.0f} s, the destructor blocked "
                "while joining its run thread with the GIL held\n"
                "stdout:\n{1}\nstderr:\n{2}".format(
                    CHILD_TIMEOUT_S, e.stdout, e.stderr
                )
            )

        detail = "returncode={0}\nstdout:\n{1}\nstderr:\n{2}".format(
            result.returncode, result.stdout, result.stderr
        )
        self.assertEqual(result.returncode, 0, detail)
        self.assertIn(marker, result.stdout, detail)

    def test_server_destructor_releases_gil_for_run_thread(self):
        self.assert_child_dropped(_SERVER_CHILD, "SERVER_DROP_OK")

    def test_client_destructor_releases_gil_for_run_thread(self):
        self.assert_child_dropped(_CLIENT_CHILD, "CLIENT_DROP_OK")


if __name__ == "__main__":
    unittest.main()
