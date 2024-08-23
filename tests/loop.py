import c104
import time

IP_ADDRESS = "10.11.1.41"  # Any invalid IP
STATION_COMMON_ADDRESS = 1


def main():
    client = c104.Client(tick_rate_ms=1000, command_timeout_ms=1000)
    connection = client.add_connection(
        ip=IP_ADDRESS, port=2404, init=c104.Init.INTERROGATION
    )
    station = connection.add_station(common_address=STATION_COMMON_ADDRESS)

    for i in range(30):
        t = time.time()
        print("Client start", i, "/", 30)
        client.start()
        client.stop()  # or client.reconnect_all()
        print("Client stopped after", round(time.time() - t, 3), "sec")

    server = c104.Server(ip="0.0.0.0", port=2404, tick_rate_ms=1000, max_connections=10)

    for i in range(30):
        t = time.time()
        print("Server start", i, "/", 30)
        server.start()
        server.stop()  # or client.reconnect_all()
        print("Server stopped after", round(time.time() - t, 3), "sec")


if __name__ == "__main__":
    main()
