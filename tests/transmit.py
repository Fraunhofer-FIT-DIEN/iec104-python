import c104
import time
import os
import psutil

# inner psutil function
def process_memory():
    process = psutil.Process(os.getpid())
    mem_info = process.memory_info()
    return mem_info.rss

def main():
    # client, connection and station preparation
    client = c104.Client()
    connection = client.add_connection(ip="127.0.0.1", port=2404, init=c104.Init.NONE)
#    connection.protocol_parameters.connection_timeout = 20
#    connection.protocol_parameters.message_timeout = 20
#    connection.protocol_parameters.confirm_interval = 10
#    connection.protocol_parameters.keep_alive_interval = 20

    # server and station preparation
    server = c104.Server()
 #   server.protocol_parameters.connection_timeout = 20
 #   server.protocol_parameters.message_timeout = 20
 #   server.protocol_parameters.confirm_interval = 10
 #   server.protocol_parameters.keep_alive_interval = 20
    station = server.add_station(common_address=47)

    # monitoring point preparation
    offset = 100
    points = []
    for i in range(700):
        print(f"add point {i}")
        points.append(station.add_point(io_address=offset+i, type=c104.Type.M_ME_NC_1, report_ms=0))

    # start
    server.start()
    client.start()

    while not server.has_active_connections:
        print("Waiting for connection")
        time.sleep(1)

    while connection.state != c104.ConnectionState.OPEN:
        print("Waiting for connection to {0}:{1}".format(connection.ip, connection.port))
        time.sleep(1)

    time.sleep(1)
    last = 0

    for i in range(10000):
        if i % 100 == 0:
            print("%04d" % (i/10,))
        for point in points:
            success = point.transmit(cause=c104.Cot.SPONTANEOUS)
            if not success:
                print(" - tx failed in %04d for IOA %s" % (i, point.io_address))
                time.sleep(0.1)

        # displaying the memory
        next = process_memory()
        if last != next:
            print(f"memory {next}")
            last = next

if __name__ == "__main__":
    # c104.set_debug_mode(c104.Debug.Server|c104.Debug.Point|c104.Debug.Callback)
    main()
