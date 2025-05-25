import asyncio
import c104
import concurrent.futures
import functools
import logging


def async_exception_handler(task: asyncio.Future):
    """
    Callback to log unhandled exceptions in scheduled async tasks.

    Required when starting asyncio coroutines from non-async (C++) callbacks,
    using `run_coroutine_threadsafe`.
    """
    try:
        task.result()
    except (asyncio.CancelledError, concurrent.futures.CancelledError):
        # Silently suppress cancellation
        return
    except Exception:
        logging.error(f"Unhandled exception in coroutine:", exc_info=True)


async def async_measurement(point: c104.Point, message: c104.IncomingMessage) -> None:
    """
    Coroutine to handle incoming measurement updates.

    Simulates processing delay or background I/O after receiving a value from the C104 server.
    """
    print(f"{point.type} MEASUREMENT received on IOA: {point.io_address}, details: {point.info}")

    # Optional debug:
    # print(f"Raw APDU: {message.raw.hex()}")
    # print("Explained:", c104.explain_bytes(apdu=message.raw))

    await asyncio.sleep(3)  # Simulate processing delay
    print("Measurement handling completed after 3 seconds")


def on_receive_point(point: c104.Point, previous_info: c104.Information,
                     message: c104.IncomingMessage, loop: asyncio.AbstractEventLoop) -> c104.ResponseState:
    """
    Synchronous C++-side callback triggered on point update.

    Uses `run_coroutine_threadsafe` to schedule the async measurement handler in the asyncio loop.
    """
    future = asyncio.run_coroutine_threadsafe(async_measurement(point, message), loop)
    future.add_done_callback(async_exception_handler)

    # Response must be returned immediately, so the coroutine does not block this call.
    return c104.ResponseState.SUCCESS


async def main():
    """
    Main async entrypoint for the client.

    Initializes a C104 connection and binds monitoring and command points.
    Demonstrates how a synchronous C++ module (`c104`) can interoperate with `asyncio`.
    """
    loop = asyncio.get_event_loop()

    # --- Client Setup ---
    client = c104.Client()
    connection = client.add_connection(ip="127.0.0.1", port=2404, init=c104.Init.ALL)
    station = connection.add_station(common_address=47)

    # --- Monitoring Point Setup ---
    point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1)

    # The callback uses `functools.partial` to bind the event loop into a synchronous function
    point.on_receive(callable=functools.partial(on_receive_point, loop=loop))

    # --- Command Point Setup ---
    command = station.add_point(io_address=13, type=c104.Type.C_DC_TA_1)

    # --- Start the Client ---
    client.start()

    while connection.state != c104.ConnectionState.OPEN:
        print(f"Waiting for connection to {connection.ip}:{connection.port}...")
        await asyncio.sleep(1)

    print(f"Client connected. Initial point value: {point.value}")

    # --- Trigger Point Read ---
    print("Triggering point read...")
    if point.read():
        print(f"-> Read SUCCESS: Value = {point.value}")
    else:
        print("-> Read FAILURE")

    await asyncio.sleep(3)

    # --- Transmit a Double Command ---
    print("Triggering double command...")
    command.info = c104.DoubleCmd(state=c104.Double.ON, qualifier=c104.Qoc.LONG_PULSE)
    if command.transmit(cause=c104.Cot.ACTIVATION):
        print("-> Command SUCCESS")
    else:
        print("-> Command FAILURE")

    # --- Keep Alive to Process Incoming Data ---
    for second in range(60):
        if connection.state != c104.ConnectionState.OPEN:
            print("Connection closed.")
            break
        print(f"Client active... ({second + 1}s)")
        await asyncio.sleep(1)


if __name__ == "__main__":
    # Optional: enable C104 debug output for troubleshooting
    # c104.set_debug_mode(c104.Debug.Client|c104.Debug.Connection|c104.Debug.Point|c104.Debug.Callback)

    logging.basicConfig(level=logging.INFO)
    asyncio.run(main())
