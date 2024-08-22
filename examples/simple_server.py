import c104
import random
import time


def on_step_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    """ handle incoming regulating step command
    """
    print("{0} STEP COMMAND on IOA: {1}, cot: {2}, previous: {3}, current: {4}".format(point.type, point.io_address, message.cot, previous_info, point.info))

    if point.value == c104.Step.LOWER:
        # do something
        return c104.ResponseState.SUCCESS

    if point.value == c104.Step.HIGHER:
        # do something
        return c104.ResponseState.SUCCESS

    return c104.ResponseState.FAILURE


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

    # monitoring point preparation
    point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
    point.on_before_auto_transmit(callable=before_auto_transmit)
    point.on_before_read(callable=before_read)

    # command point preparation
    command = station.add_point(io_address=12, type=c104.Type.C_RC_TA_1)
    command.on_receive(callable=on_step_command)

    # start
    server.start()

    while not server.has_active_connections:
        print("Waiting for connection")
        time.sleep(1)

    time.sleep(1)

    c = 0
    while server.has_open_connections and c<30:
        c += 1
        print("Keep alive until disconnected")
        time.sleep(1)


if __name__ == "__main__":
    # c104.set_debug_mode(c104.Debug.Server|c104.Debug.Point|c104.Debug.Callback)
    main()
