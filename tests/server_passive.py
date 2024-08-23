import c104
import time


def main():
    # server and station preparation
    server = c104.Server(ip="127.0.0.1", port=2404)
    station = server.add_station(common_address=47)

    # monitoring point preparation
    point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=15000)

    # command point preparation
    command = station.add_point(io_address=12, type=c104.Type.C_RC_TA_1)

    # start
    server.start()

    print("Keep alive until CTRL+C")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("quit server")


if __name__ == "__main__":
    c104.set_debug_mode(c104.Debug.Server)
    main()
    time.sleep(1)
