#!/usr/bin/env python3
"""
Copyright 2026 Fraunhofer Institute for Applied Information Technology FIT

This file is part of iec104-python.
... (GPL header)

Regression tests for SECURITY.md M12: Server.has_open_connections and
Server.open_connection_count resolved their value with
CS104_Slave_getOpenConnections(), which takes lib60870's openConnectionsLock.
The listening thread holds that lock while a freshly accepted peer completes its
TLS handshake, so a silent TCP peer - which never sends a ClientHello, so the
handshake never finishes - froze every python caller of either property while it
held the GIL, and a TransportSecurity.on_event callback, which already runs on
the listening thread inside that lock, deadlocked against itself because the
lock is a non-recursive semaphore.

Both getters now read a counter that Server::connectionEventHandler maintains
under connection_mutex.

The freeze and the self-deadlock scenarios run in a child interpreter: with the
defect the child blocks inside the C++ getter while holding the GIL, so the
parent cannot rescue it - it is killed by the subprocess timeout instead of
exiting. The children that leave a peer parked in a TLS handshake end with
os._exit(), because CS104_Slave_stop() waits for the listening thread, which
cannot finish that handshake (see SECURITY.md M12).

Run this suite like the other ones, with the installed c104 package and from a
directory other than the repo root:
    cd /tmp && python3 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_server_open_connections.py' -v
"""

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

CERTS_DIR = Path(__file__).resolve().parent / "certs"

CA_CERT = CERTS_DIR / "ca.crt"
SERVER_CERT = CERTS_DIR / "server1.crt"
SERVER_KEY = CERTS_DIR / "server1.key"
CLIENT_CERT = CERTS_DIR / "client1.crt"


def require_certs():
    missing = [
        p.name for p in (CA_CERT, SERVER_CERT, SERVER_KEY, CLIENT_CERT) if not p.exists()
    ]
    if missing:
        raise unittest.SkipTest(f"TLS test certificates missing in {CERTS_DIR}: {missing}")


CHILD_TIMEOUT_S = 20.0

# One port per scenario: a child that is killed by the timeout can leave a peer
# in TIME_WAIT, which would make the next scenario fail to bind.
SILENT_PEER_PORT = 26413
TLS_EVENT_PORT = 26414
COUNT_PORT = 26415

# Each iteration is one read of both properties. The count is picked so the loop
# runs for about a second, orders of magnitude longer than the listening thread
# needs to accept the peer and enter the handshake - so the reads do overlap the
# blocked handshake - while staying far below CHILD_TIMEOUT_S.
READ_ITERATIONS = 500000


_TLS_PREAMBLE = """
import os
import socket
import sys
import threading
import time

import c104

# The GIL is only handed over on request, and that request is made once the
# switch interval elapsed. With the default interval a blocking getter could be
# hidden by a thread switch; a long interval keeps the thread that is inside the
# C++ getter on the GIL.
sys.setswitchinterval(10.0)


def tls_config(certs_dir):
    tls = c104.TransportSecurity(validate=True, only_known=True)
    tls.set_certificate(
        cert=os.path.join(certs_dir, "server1.crt"),
        key=os.path.join(certs_dir, "server1.key"),
    )
    tls.set_ca_certificate(cert=os.path.join(certs_dir, "ca.crt"))
    tls.set_version(min=c104.TlsVersion.TLS_1_2, max=c104.TlsVersion.TLS_1_3)
    tls.add_allowed_remote_certificate(cert=os.path.join(certs_dir, "client1.crt"))
    tls.set_hostname_verification(hostname="Local Test Client 1")
    return tls
"""


_SILENT_PEER_CHILD = (
    _TLS_PREAMBLE
    + """
certs_dir = {certs!r}
port = {port}

server = c104.Server(
    ip="127.0.0.1",
    port=port,
    tick_rate_ms=50,
    transport_security=tls_config(certs_dir),
)
server.start()

# A plain TCP peer that never speaks TLS: the listening thread accepts it and
# blocks inside the handshake, which it does while holding
# openConnectionsLock.
peer = socket.create_connection(("127.0.0.1", port), timeout=10.0)

result = []


def read_properties():
    for _ in range({reads}):
        assert server.has_open_connections is False
        assert server.open_connection_count == 0
    result.append("done")


reader = threading.Thread(target=read_properties, daemon=True)
reader.start()

# With the defect the reader never returns and keeps the GIL, so even this join
# cannot expire - the child hangs until the parent's subprocess timeout kills it.
reader.join(timeout=10.0)
assert not reader.is_alive(), "property read blocked behind the TLS handshake"
assert result == ["done"], "responsiveness loop did not complete"

peer.close()
print("SILENT_PEER_OK", flush=True)

os._exit(0)
""".format(certs=str(CERTS_DIR), port=SILENT_PEER_PORT, reads=READ_ITERATIONS)
)


_TLS_EVENT_CHILD = (
    _TLS_PREAMBLE
    + """
certs_dir = {certs!r}
port = {port}

observed = []
event_seen = threading.Event()

server = None


def on_tls_event(tls: c104.TransportSecurity, level: c104.TlsEventLevel, code: int, message: str,
                 peer: str) -> None:
    # Runs on the listening thread inside lib60870's openConnectionsLock: the
    # getters must not take that lock again, which is the self-deadlock case.
    observed.append((bool(server.has_open_connections), int(server.open_connection_count)))
    event_seen.set()


tls = tls_config(certs_dir)
tls.on_event(callable=on_tls_event)

server = c104.Server(
    ip="127.0.0.1",
    port=port,
    tick_rate_ms=50,
    transport_security=tls,
)
server.start()

# Garbage instead of a ClientHello: mbedtls rejects it and the TLS security
# event fires from inside the handshake, while the accept thread holds the lock.
peer = socket.create_connection(("127.0.0.1", port), timeout=10.0)
peer.sendall(b"not a tls client hello")

assert event_seen.wait(10.0), "no TLS event was reported for the failed handshake"
assert observed, "TLS event callback did not run"
assert all(state == (False, 0) for state in observed), (
    "unexpected counter state inside on_event: %r" % (observed,)
)

print("TLS_EVENT_OK", flush=True)
os._exit(0)
""".format(certs=str(CERTS_DIR), port=TLS_EVENT_PORT)
)


_COUNT_CHILD = """
import time

import c104


def wait_for(predicate, timeout_s=10.0, interval_s=0.05):
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(interval_s)
    return False


port = {port}

server = c104.Server(ip="127.0.0.1", port=port, tick_rate_ms=50)
client = c104.Client(tick_rate_ms=50)
connection = client.add_connection(ip="127.0.0.1", port=port, init=c104.Init.NONE)

server.start()
client.start()

assert wait_for(
    lambda: server.open_connection_count == 1 and server.has_open_connections
), "server did not count the opened connection"

assert wait_for(
    lambda: server.active_connection_count == 1 and server.has_active_connections
), "connection never became active"

assert connection.mute(), "connection was not open, cannot mute it"
assert wait_for(
    lambda: server.active_connection_count == 0 and not server.has_active_connections
), "STOPDT did not deactivate the connection"
assert server.open_connection_count == 1, "STOPDT must not change the open count"
assert server.has_open_connections

assert connection.unmute(), "connection was not open, cannot unmute it"
assert wait_for(
    lambda: server.active_connection_count == 1 and server.has_active_connections
), "STARTDT did not reactivate the connection"
assert server.open_connection_count == 1

client.stop()
assert wait_for(
    lambda: server.open_connection_count == 0 and not server.has_open_connections
), "closed connection is still counted as open"
assert server.active_connection_count == 0 and not server.has_active_connections

server.stop()
assert server.open_connection_count == 0 and not server.has_open_connections
assert server.active_connection_count == 0 and not server.has_active_connections

server.start()
assert server.open_connection_count == 0 and not server.has_open_connections
assert server.active_connection_count == 0 and not server.has_active_connections
server.stop()

print("COUNT_OK", flush=True)
""".format(port=COUNT_PORT)


class SubprocessServerTestCase(unittest.TestCase):
    def run_child(self, script):
        workdir = tempfile.mkdtemp(prefix="c104-server-open-connections-")
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

    def assert_child_marker(self, script, marker):
        try:
            result = self.run_child(script)
        except subprocess.TimeoutExpired as e:
            self.fail(
                "child did not exit within {0:.0f} s: a getter blocked on "
                "lib60870's openConnectionsLock\nstdout:\n{1}\nstderr:\n{2}".format(
                    CHILD_TIMEOUT_S, e.stdout, e.stderr
                )
            )

        detail = "returncode={0}\nstdout:\n{1}\nstderr:\n{2}".format(
            result.returncode, result.stdout, result.stderr
        )
        self.assertEqual(result.returncode, 0, detail)
        self.assertIn(marker, result.stdout, detail)


class ServerOpenConnectionFreezeTestCase(SubprocessServerTestCase):
    def test_silent_tls_peer_does_not_block_property_reads(self):
        require_certs()
        self.assert_child_marker(_SILENT_PEER_CHILD, "SILENT_PEER_OK")

    def test_property_reads_from_tls_event_do_not_deadlock(self):
        require_certs()
        self.assert_child_marker(_TLS_EVENT_CHILD, "TLS_EVENT_OK")


class ServerOpenConnectionCountTestCase(SubprocessServerTestCase):
    def test_counts_across_open_startdt_stopdt_close_and_restart(self):
        self.assert_child_marker(_COUNT_CHILD, "COUNT_OK")


if __name__ == "__main__":
    unittest.main()
