import c104
import random
import time


def before_auto_transmit(point: c104.Point) -> None:
    """ update point value before transmission
    """
    point.value = random.random() * 100
    print("{0} BEFORE AUTOMATIC REPORT on IOA: {1} VALUE: {2}".format(point.type, point.io_address, point.value))


def before_read(point: c104.Point) -> None:
    """ update point value before transmission
    """
    point.value = random.random() * 100
    print("{0} BEFORE READ or INTERROGATION on IOA: {1} VALUE: {2}".format(point.type, point.io_address, point.value))


def main():
    # server and station preparation
    server = c104.Server()
    station = server.add_station(common_address=47)

    # monitoring point preparation - sequence
    offset = 8
    for i in range(30):
        print(f"add point {i}")
        pt = station.add_point(io_address=offset+i, type=c104.Type.M_ME_NC_1, report_ms=30000)
        pt.on_before_auto_transmit(callable=before_auto_transmit)
        pt.on_before_read(callable=before_read)

    # non-sequence
    point4 = station.add_point(io_address=40, type=c104.Type.M_ME_TE_1, report_ms=30000)
    point4.value = c104.Int16(100)
    point5 = station.add_point(io_address=43, type=c104.Type.M_ME_TE_1, report_ms=30000)
    point5.value = c104.Int16(200)
    point6 = station.add_point(io_address=44, type=c104.Type.M_ME_TE_1, report_ms=30000)
    point6.value = c104.Int16(300)
    point7 = station.add_point(io_address=45, type=c104.Type.M_ME_TE_1, report_ms=30000)
    point7.value = c104.Int16(400)

    batch = c104.Batch(cause=c104.Cot.SPONTANEOUS, points=[point5, point6])
    batch.add_point(point7)
    print(batch)

    # start
    server.start()

    while not server.has_active_connections:
        print("Waiting for connection")
        time.sleep(1)

    time.sleep(1)

    print("transmit batch 1")
    server.transmit_batch(batch)

    time.sleep(1)

    print("transmit batch 2")
    server.transmit_batch(batch)

    time.sleep(1)

    c = 0
    while server.has_open_connections and c<30:
        c += 1
        print("Keep alive until disconnected")
        time.sleep(1)


if __name__ == "__main__":
    print()
    print("START batch server")
    print()
    c104.set_debug_mode(c104.Debug.Server|c104.Debug.Message)
    main()
