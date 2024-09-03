import c104
import time
import random


def on_before_auto_transmit_step(point: c104.Point) -> None:
    print("SV] {0} PERIODIC TRANSMIT on IOA: {1}".format(point.type, point.io_address))
    point.value = c104.Int7(random.randint(-64,63))  # import random

def main():
    # server and station preparation
    server = c104.Server(ip="127.0.0.1", port=2404)
    station = server.add_station(common_address=47)

    # monitoring point preparation
    p1 = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=500)
    p2 = station.add_point(io_address=14, type=c104.Type.M_ST_TB_1, report_ms=500)
    p2.on_before_auto_transmit(callable=on_before_auto_transmit_step)

    c1 = station.add_point(io_address=15, type=c104.Type.C_RC_TA_1)

    # start
    server.start()

    print("Keep alive until CTRL+C")

    try:
        while True:
            time.sleep(1)
            print("open %s active %s" % (server.open_connection_count, server.active_connection_count))
    except KeyboardInterrupt:
        print("quit server")


if __name__ == "__main__":
    c104.set_debug_mode(c104.Debug.Server|c104.Debug.Point|c104.Debug.Callback)
    main()
    time.sleep(1)
