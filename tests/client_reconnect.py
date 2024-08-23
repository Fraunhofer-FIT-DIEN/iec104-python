import c104
import time

IP_ADDRESS = "127.0.0.1" # Any invalid IP
STATION_COMMON_ADDRESS = 47


def main():
    client = c104.Client(tick_rate_ms=100, command_timeout_ms=5000)
    connection = client.add_connection(
        ip=IP_ADDRESS, port=2404, init=c104.Init.INTERROGATION
    )
    station = connection.add_station(common_address=STATION_COMMON_ADDRESS)

    try:
        while True:
            print("Loop Start")
            client.start()
            print("Loop Started")
            time.sleep(1)
            connection.disconnect()
            print("Disconnected")
            client.stop() # or client.reconnect_all()
            print("Loop End") # This never occurs, because the script will stop at client.stop()
    except KeyboardInterrupt:
        print("quit client")


if __name__ == "__main__":
    c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection)
    main()
    time.sleep(1)
