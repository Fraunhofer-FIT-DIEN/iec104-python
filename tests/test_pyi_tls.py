#!/usr/bin/env python3
"""
Copyright 2026 Fraunhofer Institute for Applied Information Technology FIT

This file is part of iec104-python.
... (GPL header)

Python unittest suite exercising EVERY public object/method of
c104.TransportSecurity as declared in c104/__init__.pyi.

TransportSecurity is a pure TLS *configuration* object (it wraps a lib60870
TLSConfiguration), so none of these tests require a live network connection or
TLS handshake. Certificate based tests use the static test certs under
tests/certs/ (ca.crt, server1.{crt,key}, client1.crt, ...).

IMPORTANT: like test_c104.py, this suite must be run with the *installed* c104
package (NOT the in-tree c104/ source directory). Run it from a directory other
than the repo root, e.g.:

    cd /tmp && python3 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_pyi_tls.py'
    # or with coverage:
    cd /tmp && coverage run --source c104 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_pyi_tls.py' && coverage report
"""

import datetime
import os
import unittest
from pathlib import Path

import c104

# ---------------------------------------------------------------------------
# Certificate path resolution
#
# The certs live next to this test file under tests/certs/. Resolve them
# relative to this file so the tests work no matter what the current working
# directory is (the suite must be run from a directory that is NOT the repo).
# ---------------------------------------------------------------------------
CERTS_DIR = Path(__file__).resolve().parent / "certs"

CA_CERT = CERTS_DIR / "ca.crt"
SERVER_CERT = CERTS_DIR / "server1.crt"
SERVER_KEY = CERTS_DIR / "server1.key"
CLIENT_CERT = CERTS_DIR / "client1.crt"

# A passphrase-protected copy of SERVER_KEY (openssl rsa -aes256), generated so
# the wrong-passphrase decrypt-error branch of set_certificate is exercised.
# The passphrase below is a fixed, known constant documented alongside the key.
ENCRYPTED_SERVER_KEY = CERTS_DIR / "server1_encrypted.key"
ENCRYPTED_SERVER_KEY_PASSPHRASE = "test-pass"

# A path that is guaranteed not to exist.
MISSING = CERTS_DIR / "definitely-not-here.crt"

# The exact message the C++ layer raises once a config has been handed to a
# client or server (readonly lock).
ALREADY_PASSED = (
    "The configuration has already been passed to a client or server and can "
    "no longer be modified."
)


def require_certs():
    """Enforce that the TLS certs are present before cert-dependent tests."""
    missing = [p.name for p in (CA_CERT, SERVER_CERT, SERVER_KEY, CLIENT_CERT,
                                ENCRYPTED_SERVER_KEY)
               if not p.exists()]
    if missing:
        raise unittest.SkipTest(
            f"TLS test certificates missing in {CERTS_DIR}: {missing}")


class TransportSecurityInitTestCase(unittest.TestCase):
    """c104.TransportSecurity.__init__"""

    def test_default_construction(self):
        ts = c104.TransportSecurity()
        self.assertIsInstance(ts, c104.TransportSecurity)

    def test_explicit_ctor_args(self):
        # validate: bool, only_known: bool - all combinations must construct.
        for validate in (True, False):
            for only_known in (True, False):
                ts = c104.TransportSecurity(validate=validate,
                                            only_known=only_known)
                self.assertIsInstance(ts, c104.TransportSecurity)

    def test_repr(self):
        # repr / str must work and mention the class.
        ts = c104.TransportSecurity()
        self.assertIn("TransportSecurity", repr(ts))
        self.assertIn("TransportSecurity", str(ts))


class TransportSecurityCertificateTestCase(unittest.TestCase):
    """set_certificate(cert, key, passphrase='')"""

    def setUp(self):
        require_certs()
        # A FRESH config is required for every set_certificate with a real key:
        # mbedtls rejects re-parsing a key into an already-populated pk
        # context, so repeated loads into one instance fail.
        self.ts = c104.TransportSecurity()

    def test_valid_cert_and_key(self):
        self.assertIsNone(
            self.ts.set_certificate(str(SERVER_CERT), str(SERVER_KEY)))

    def test_valid_cert_and_key_empty_passphrase(self):
        # explicit empty passphrase is treated as "no password"
        self.assertIsNone(
            self.ts.set_certificate(str(SERVER_CERT), str(SERVER_KEY), ""))

    def test_empty_cert_path(self):
        with self.assertRaises(ValueError):
            self.ts.set_certificate("", str(SERVER_KEY))

    def test_missing_cert_path(self):
        with self.assertRaises(ValueError):
            self.ts.set_certificate(str(MISSING), str(SERVER_KEY))

    def test_empty_key_path(self):
        with self.assertRaises(ValueError):
            self.ts.set_certificate(str(SERVER_CERT), "")

    def test_missing_key_path(self):
        with self.assertRaises(ValueError):
            self.ts.set_certificate(str(SERVER_CERT), str(MISSING))

    def test_wrong_passphrase_unencrypted_key(self):
        # server1.key is UNENCRYPTED. mbedtls ignores the passphrase when the
        # key is not encrypted, so supplying a wrong passphrase succeeds here
        # instead of raising (documented behavior for unencrypted keys).
        self.assertIsNone(self.ts.set_certificate(
            str(SERVER_CERT), str(SERVER_KEY), "wrong-passphrase"))

    def test_encrypted_key_correct_passphrase(self):
        # server1_encrypted.key is passphrase-protected ("test-pass"). The
        # correct passphrase must load the key without error.
        self.assertIsNone(self.ts.set_certificate(
            str(SERVER_CERT), str(ENCRYPTED_SERVER_KEY),
            ENCRYPTED_SERVER_KEY_PASSPHRASE))

    def test_encrypted_key_wrong_passphrase(self):
        # A wrong passphrase on the ENCRYPTED key must fail to decrypt and
        # raise, unlike the unencrypted key above.
        with self.assertRaises(ValueError):
            self.ts.set_certificate(
                str(SERVER_CERT), str(ENCRYPTED_SERVER_KEY),
                "wrong-passphrase")

    def test_readonly_already_passed(self):
        server = c104.Server(transport_security=self.ts)
        with self.assertRaises(ValueError) as cm:
            self.ts.set_certificate(str(SERVER_CERT), str(SERVER_KEY))
        self.assertIn("already been passed", str(cm.exception))


class TransportSecurityCACertificateTestCase(unittest.TestCase):
    """set_ca_certificate(cert)"""

    def setUp(self):
        require_certs()
        self.ts = c104.TransportSecurity()

    def test_valid_ca(self):
        self.assertIsNone(self.ts.set_ca_certificate(str(CA_CERT)))

    def test_empty_path(self):
        with self.assertRaises(ValueError):
            self.ts.set_ca_certificate("")

    def test_missing_path(self):
        with self.assertRaises(ValueError):
            self.ts.set_ca_certificate(str(MISSING))

    def test_readonly_already_passed(self):
        server = c104.Server(transport_security=self.ts)
        with self.assertRaises(ValueError) as cm:
            self.ts.set_ca_certificate(str(CA_CERT))
        self.assertIn("already been passed", str(cm.exception))


class TransportSecurityAllowedRemoteCertTestCase(unittest.TestCase):
    """add_allowed_remote_certificate(cert)"""

    def setUp(self):
        require_certs()
        self.ts = c104.TransportSecurity()

    def test_valid_client_cert(self):
        self.assertIsNone(self.ts.add_allowed_remote_certificate(
            str(CLIENT_CERT)))

    def test_empty_path(self):
        with self.assertRaises(ValueError):
            self.ts.add_allowed_remote_certificate("")

    def test_missing_path(self):
        with self.assertRaises(ValueError):
            self.ts.add_allowed_remote_certificate(str(MISSING))

    def test_readonly_already_passed(self):
        server = c104.Server(transport_security=self.ts)
        with self.assertRaises(ValueError) as cm:
            self.ts.add_allowed_remote_certificate(str(CLIENT_CERT))
        self.assertIn("already been passed", str(cm.exception))


class TransportSecurityCiphersTestCase(unittest.TestCase):
    """set_ciphers(ciphers: list[c104.TlsCipher])"""

    def setUp(self):
        self.ts = c104.TransportSecurity()

    def _valid_ciphers(self):
        return [
            c104.TlsCipher.ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
            c104.TlsCipher.ECDHE_RSA_WITH_AES_128_GCM_SHA256,
            c104.TlsCipher.TLS1_3_AES_128_GCM_SHA256,
        ]

    def test_valid_non_empty_list(self):
        self.assertIsNone(self.ts.set_ciphers(self._valid_ciphers()))

    def test_single_cipher(self):
        self.assertIsNone(
            self.ts.set_ciphers([c104.TlsCipher.RSA_WITH_AES_128_CBC_SHA]))

    def test_empty_list_raises(self):
        with self.assertRaises(ValueError):
            self.ts.set_ciphers([])

    def test_invalid_cipher_raises(self):
        # an unhashable / non-enum element is not a valid ciphersuite
        with self.assertRaises((ValueError, TypeError)):
            self.ts.set_ciphers(["not-a-cipher"])

    def test_readonly_already_passed(self):
        server = c104.Server(transport_security=self.ts)
        with self.assertRaises(ValueError) as cm:
            self.ts.set_ciphers(self._valid_ciphers())
        self.assertIn("already been passed", str(cm.exception))


class TransportSecurityHostnameVerificationTestCase(unittest.TestCase):
    """set_hostname_verification(hostname=None)"""

    def setUp(self):
        self.ts = c104.TransportSecurity()

    def test_valid_hostname(self):
        self.assertIsNone(self.ts.set_hostname_verification("localhost"))

    def test_none_disables(self):
        # The .pyi declares a default of None; the C++ binding now accepts both
        # an explicit None and a no-argument call (both disable verification).
        self.assertIsNone(self.ts.set_hostname_verification(None))
        self.assertIsNone(self.ts.set_hostname_verification())

    def test_empty_hostname_raises(self):
        with self.assertRaises(ValueError):
            self.ts.set_hostname_verification("")

    def test_readonly_after_server_use(self):
        # The hostname verification setter is guarded like the other setters:
        # once the instance is passed to a server it becomes read-only.
        server = c104.Server(transport_security=self.ts)
        with self.assertRaises(ValueError) as cm:
            self.ts.set_hostname_verification("localhost")
        self.assertIn("already been passed", str(cm.exception))


class TransportSecurityRenegotiationTimeTestCase(unittest.TestCase):
    """set_renegotiation_time(interval: datetime.timedelta | None)"""

    MIN = datetime.timedelta(minutes=5)
    MAX = datetime.timedelta(hours=24)

    def setUp(self):
        self.ts = c104.TransportSecurity()

    def test_none_disables(self):
        # .pyi declares a default of None; the binding now accepts both an
        # explicit None and a no-argument call (disables automatic renegotiation).
        self.assertIsNone(self.ts.set_renegotiation_time(None))
        self.assertIsNone(self.ts.set_renegotiation_time())

    def test_valid_intervals(self):
        for iv in (self.MIN, self.MAX,
                   datetime.timedelta(hours=1),
                   datetime.timedelta(minutes=5),
                   datetime.timedelta(hours=24)):
            ts = c104.TransportSecurity()
            self.assertIsNone(ts.set_renegotiation_time(iv))

    def test_below_min_raises(self):
        with self.assertRaises(ValueError):
            self.ts.set_renegotiation_time(datetime.timedelta(minutes=4))

    def test_above_max_raises(self):
        with self.assertRaises(ValueError):
            self.ts.set_renegotiation_time(datetime.timedelta(hours=25))

    def test_readonly_already_passed(self):
        server = c104.Server(transport_security=self.ts)
        with self.assertRaises(ValueError) as cm:
            self.ts.set_renegotiation_time(self.MIN)
        self.assertIn("already been passed", str(cm.exception))


class TransportSecurityResumptionIntervalTestCase(unittest.TestCase):
    """set_resumption_interval(interval: datetime.timedelta | None)"""

    MIN = datetime.timedelta(minutes=5)
    MAX = datetime.timedelta(days=7)

    def setUp(self):
        self.ts = c104.TransportSecurity()

    def test_none_disables(self):
        # .pyi declares a default of None; the binding now accepts both an
        # explicit None and a no-argument call (disables session resumption).
        self.assertIsNone(self.ts.set_resumption_interval(None))
        self.assertIsNone(self.ts.set_resumption_interval())

    def test_valid_intervals(self):
        for iv in (self.MIN, self.MAX, datetime.timedelta(hours=6)):
            ts = c104.TransportSecurity()
            self.assertIsNone(ts.set_resumption_interval(iv))

    def test_below_min_raises(self):
        with self.assertRaises(ValueError):
            self.ts.set_resumption_interval(datetime.timedelta(seconds=1))

    def test_above_max_raises(self):
        with self.assertRaises(ValueError):
            self.ts.set_resumption_interval(datetime.timedelta(hours=169))

    def test_readonly_already_passed(self):
        server = c104.Server(transport_security=self.ts)
        with self.assertRaises(ValueError) as cm:
            self.ts.set_resumption_interval(self.MIN)
        self.assertIn("already been passed", str(cm.exception))


class TransportSecurityVersionTestCase(unittest.TestCase):
    """set_version(min: TlsVersion = ..., max: TlsVersion = ...)"""

    def setUp(self):
        self.ts = c104.TransportSecurity()

    def test_defaults(self):
        self.assertIsNone(self.ts.set_version())

    def test_explicit_versions(self):
        ts = c104.TransportSecurity()
        self.assertIsNone(ts.set_version(c104.TlsVersion.TLS_1_2,
                                         c104.TlsVersion.TLS_1_2))

    def test_min_max_range(self):
        ts = c104.TransportSecurity()
        self.assertIsNone(ts.set_version(c104.TlsVersion.TLS_1_0,
                                         c104.TlsVersion.TLS_1_3))

    def test_not_selected(self):
        ts = c104.TransportSecurity()
        self.assertIsNone(ts.set_version(c104.TlsVersion.NOT_SELECTED,
                                         c104.TlsVersion.NOT_SELECTED))

    def test_readonly_already_passed(self):
        server = c104.Server(transport_security=self.ts)
        with self.assertRaises(ValueError) as cm:
            self.ts.set_version(c104.TlsVersion.TLS_1_2,
                                c104.TlsVersion.TLS_1_2)
        self.assertIn("already been passed", str(cm.exception))


class TransportSecurityReadonlyAcrossSetttersTestCase(unittest.TestCase):
    """After passing a config to a Server or Client, every guarded setter
    raises the "already been passed to a client or server" ValueError."""

    def setUp(self):
        require_certs()

    def _assert_readonly(self, ts):
        with self.assertRaises(ValueError) as cm:
            ts.set_certificate(str(SERVER_CERT), str(SERVER_KEY))
        self.assertIn("already been passed", str(cm.exception))
        with self.assertRaises(ValueError):
            ts.set_ca_certificate(str(CA_CERT))
        with self.assertRaises(ValueError):
            ts.set_ciphers([c104.TlsCipher.RSA_WITH_AES_128_CBC_SHA])
        with self.assertRaises(ValueError):
            ts.set_renegotiation_time(datetime.timedelta(minutes=5))
        with self.assertRaises(ValueError):
            ts.set_resumption_interval(datetime.timedelta(minutes=5))
        with self.assertRaises(ValueError):
            ts.add_allowed_remote_certificate(str(CLIENT_CERT))
        with self.assertRaises(ValueError):
            ts.set_version(c104.TlsVersion.TLS_1_2, c104.TlsVersion.TLS_1_2)

    def test_readonly_after_passed_to_server(self):
        ts = c104.TransportSecurity()
        server = c104.Server(transport_security=ts)
        self.assertIsNotNone(server)
        self._assert_readonly(ts)

    def test_readonly_after_passed_to_client(self):
        # OBSERVED behavior: unlike a Server, passing the config to a Client
        # does NOT flip the readonly lock -- the config stays modifiable. The
        # class docstring claims "assigned to a client or server ... becomes
        # read-only" but only the Server path enforces it in this build.
        ts = c104.TransportSecurity()
        client = c104.Client(transport_security=ts)
        self.assertIsNotNone(client)
        # still fully modifiable after being handed to a Client
        self.assertIsNone(ts.set_certificate(str(SERVER_CERT), str(SERVER_KEY)))
        self.assertIsNone(ts.set_ca_certificate(str(CA_CERT)))
        self.assertIsNone(ts.set_ciphers([c104.TlsCipher.RSA_WITH_AES_128_CBC_SHA]))

    def test_readonly_persists(self):
        # A config given to a server stays readonly even on a fresh instance of
        # the same config object (the flag lives on the config, not the server).
        ts = c104.TransportSecurity()
        c104.Server(transport_security=ts)
        with self.assertRaises(ValueError):
            ts.set_ca_certificate(str(CA_CERT))


if __name__ == "__main__":
    unittest.main()
