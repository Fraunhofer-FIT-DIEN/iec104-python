import c104
import random
import time


def main():
    # client, connection and station preparation
    client = c104.Client()
    connection = client.add_connection(ip="127.0.0.1", port=2404, init=c104.Init.INTERROGATION)
    station = connection.add_station(common_address=47)

    # monitoring point preparation
    point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1)

    # command point preparation
    command = station.add_point(io_address=12, type=c104.Type.C_RC_TA_1)
    command.value = c104.Step.HIGHER

    # start
    client.start()

    while not connection.is_connected:
        print("Waiting for connection to {0}:{1}".format(connection.ip, connection.port))
        time.sleep(1)

    #time.sleep(3)

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
    #time.sleep(3)

    print("exit")
    print("exit")
    print("exit")


if __name__ == "__main__":
    c104.set_debug_mode(c104.Debug.Client|c104.Debug.Connection|c104.Debug.Point|c104.Debug.Callback)
    main()
    time.sleep(1)
