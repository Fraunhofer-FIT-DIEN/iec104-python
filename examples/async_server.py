import asyncio
import c104
import concurrent.futures
import functools
import logging
import random


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
        pass
    except Exception:
        logging.error(f"Unhandled exception in coroutine:", exc_info=True)


async def async_command(point: c104.Point):
    """
    Coroutine to process a received double command from the C104 stack.

    This simulates a long-running operation in response to a command.
    """
    print(f"{point.type} DOUBLE COMMAND received on IOA: {point.io_address}, details: {point.info}")
    if isinstance(point.info, c104.DoubleCmd) and point.info.qualifier == c104.Qoc.LONG_PULSE:
        print("------> Received LONG PULSE")

    # Simulated step-wise processing (can be replaced with I/O-bound operations)
    await asyncio.sleep(1)
    print("async_cmd: after sleep 1s")
    await asyncio.sleep(5)
    print("async_cmd: after sleep 5s")
    await asyncio.sleep(1)
    print("async_cmd: after sleep final 1s")


def on_double_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage,
                      loop: asyncio.AbstractEventLoop) -> c104.ResponseState:
    """
    Synchronous callback for C104 command point, triggered by the server.

    Schedules an async coroutine (`async_cmd`) using the event loop.
    Because this is a synchronous context (C++-backed), we use `run_coroutine_threadsafe`.
    """
    future = asyncio.run_coroutine_threadsafe(async_command(point), loop)
    future.add_done_callback(async_exception_handler)

    # Response must be returned immediately, so the coroutine does not block this call.
    return c104.ResponseState.SUCCESS


def random_value(point: c104.Point) -> None:
    """
    Synchronous callback to update a monitoring point before it is transmitted.

    C104 expects the updated value immediately, so this must remain non-async.
    """
    point.value = random.random() * 100  # Simulate a measurement


async def main():
    """
    Main async entrypoint for the server.

    Initializes a C104 server and binds monitoring and command points.
    Demonstrates how a synchronous C++ module (`c104`) can interoperate with `asyncio`.
    """
    loop = asyncio.get_event_loop()

    # --- Server Setup ---
    server = c104.Server()
    station = server.add_station(common_address=47)

    # --- Monitoring Point Setup ---
    point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=10000)
    point.on_before_auto_transmit(callable=random_value)
    point.on_before_read(callable=random_value)

    # --- Command Point Setup ---
    command = station.add_point(io_address=13, type=c104.Type.C_DC_TA_1)
    command.value = c104.Double.OFF

    # The callback uses `functools.partial` to bind the event loop into a synchronous function
    command.on_receive(callable=functools.partial(on_double_command, loop=loop))

    # --- Start the Server ---
    server.start()

    # Wait until a client connects
    while not server.has_active_connections:
        print("Waiting for client connection...")
        await asyncio.sleep(1)

    print("Client connected.")
    await asyncio.sleep(1)

    # if you just want to wait forever uncomment the following two lines and remove the rest of this function
    # forever = asyncio.Event()
    # await forever.wait()

    for second in range(30):
        if not server.has_open_connections:
            print("Connection closed.")
            break
        print(f"Server active... ({second + 1}s)")
        await asyncio.sleep(1)


if __name__ == "__main__":
    # Optional: enable C104 debug output for troubleshooting
    # c104.set_debug_mode(c104.Debug.Server|c104.Debug.Point|c104.Debug.Callback)

    logging.basicConfig(level=logging.INFO)
    asyncio.run(main())
