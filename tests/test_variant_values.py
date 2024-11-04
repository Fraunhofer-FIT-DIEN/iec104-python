import logging
import random
import c104


def on_before_auto_transmit_step(point: c104.Point) -> None:
    print("SV] {0} PERIODIC TRANSMIT on IOA: {1}".format(point.type, point.io_address))
    point.value = c104.Int7(random.randint(-64,63))  # import random

def main():
    # server and station preparation
    server = c104.Server(ip="127.0.0.1", port=2404)
    station = server.add_station(common_address=47)

    i = 0

    # test point value setter with variadic type
    for name, type_id in c104.Type.__members__.items():
        try:
            point = station.add_point(io_address=i, type=type_id)
            try:
                logging.info("Point(%s): %s has value class %s", i, name, point.value.__class__)

                logging.info(" -> old value: %s - %s", point.value, point.quality)
                if isinstance(point.value, c104.Byte32):
                    x = c104.Byte32(0b10101000011100101010000111010101)
                else:
                    x = point.value.__class__(int(point.value)+1)
                y = None if point.quality is None else point.quality.__class__(int(point.quality)+1)
                point.value = x
                point.quality = y
                logging.info(" -> new value: %s - %s", point.value, point.quality)

            except ValueError as e:
                logging.error("Point(%s): %s failed with %s" , i, name, e)
                logging.warning(" -> old value: %s - %s", point.value, point.quality)
                logging.warning(" -> new value: %s - %s", point.value, point.quality)

        except ValueError as e:
            if 'Unsupported' in str(e):
                logging.warning("Point(%s): %s failed with %s" % (i, name, e))
            else:
                logging.error("Point(%s): %s failed with %s" % (i, name, e))

        i += 1


if __name__ == "__main__":
#    c104.set_debug_mode(c104.Debug.Server|c104.Debug.Point|c104.Debug.Callback)
    logging.basicConfig(level=logging.WARNING)
    main()
