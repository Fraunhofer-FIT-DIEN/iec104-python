import time
import unittest

import c104


class TestOriginatorAddress(unittest.TestCase):
    def setUp(self) -> None:
        self.server = c104.Server(ip="127.0.0.1", port=2404, tick_rate_ms=100)
        self.station = self.server.add_station(common_address=1)
        self.server_point = self.station.add_point(io_address=1, type=c104.Type.M_SP_NA_1)
        self.server_point.value = True
        self.server.start()

        self.client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        self.connection = self.client.add_connection(
            ip="127.0.0.1",
            port=2404,
            init=c104.Init.NONE,
        )
        self.client_station = self.connection.add_station(common_address=1)
        self.client_point = self.client_station.add_point(io_address=1, type=c104.Type.M_SP_NA_1)
        self.client.start()

        self.received_originators = []

        def on_receive_point(
            point: c104.Point,
            previous_info: c104.Information,
            message: c104.IncomingMessage,
        ) -> c104.ResponseState:
            self.received_originators.append(message.originator_address)
            return c104.ResponseState.SUCCESS

        self.client_point.on_receive(callable=on_receive_point)

        self.assertTrue(self._wait_for(lambda: self.connection.is_connected, timeout_s=5))

    def tearDown(self) -> None:
        if hasattr(self, "client") and self.client:
            self.client.disconnect_all()
        if hasattr(self, "server") and self.server:
            self.server.stop()

    def _wait_for(self, predicate, timeout_s: float) -> bool:
        start = time.time()
        while time.time() - start < timeout_s:
            if predicate():
                return True
            time.sleep(0.05)
        return False

    def _wait_for_messages(self, min_count: int, timeout_s: float = 5.0) -> bool:
        return self._wait_for(lambda: len(self.received_originators) >= min_count, timeout_s)

    def test_originator_address_per_command(self) -> None:
        self.connection.originator_address = 12
        self.received_originators.clear()
        self.assertTrue(self.connection.interrogation(common_address=1, wait_for_response=True))
        self.assertTrue(self._wait_for_messages(1))
        self.assertTrue(all(oa == 12 for oa in self.received_originators))

        self.connection.originator_address = 34
        self.received_originators.clear()
        self.assertTrue(self.client_point.read())
        self.assertTrue(self._wait_for_messages(1))
        self.assertTrue(all(oa == 34 for oa in self.received_originators))


if __name__ == "__main__":
    unittest.main()
