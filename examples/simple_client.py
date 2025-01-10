import c104
import random
import time


def con_on_unexpected_message(connection: c104.Connection, message: c104.IncomingMessage, cause: c104.Umc) -> None:
    if cause == c104.Umc.MISMATCHED_TYPE_ID :
        station = connection.get_station(message.common_address)
        if station:
            point = station.get_point(message.io_address)
            if point:
                print("CL] <-in-- CONFLICT | SERVER CA {0} reports IOA {1} type as {2}, but is already registered as {3}".format(message.common_address, message.io_address, message.type, point.type))
                return
    print("CL] <-in-- REJECTED | {1} from SERVER CA {0}".format(message.common_address, cause))

def main():
    # client, connection and station preparation
    client = c104.Client()
    connection = client.add_connection(ip="127.0.0.1", port=2404, init=c104.Init.ALL)
    connection.on_unexpected_message(callable=con_on_unexpected_message)
    station = connection.add_station(common_address=47)

    # monitoring point preparation
    point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1)

    # command point preparation
    command = station.add_point(io_address=12, type=c104.Type.C_RC_TA_1)
    command.value = c104.Step.HIGHER

    # start
    client.start()

    while connection.state != c104.ConnectionState.OPEN:
        print("Waiting for connection to {0}:{1}".format(connection.ip, connection.port))
        time.sleep(1)

    print(f"-> AFTER INIT {point.value}")

    print("read")
    print("read")
    print("read")
    if point.read():
        print(f"-> SUCCESS {point.value}")
    else:
        print("-> FAILURE")

    #time.sleep(3)

    print("transmit")
    print("transmit")
    print("transmit")
    if command.transmit(cause=c104.Cot.ACTIVATION):
        print("-> SUCCESS")
    else:
        print("-> FAILURE")

    time.sleep(3)

    print("exit")
    print("exit")
    print("exit")


if __name__ == "__main__":
    # c104.set_debug_mode(c104.Debug.Client|c104.Debug.Connection|c104.Debug.Point|c104.Debug.Callback)
    main()
