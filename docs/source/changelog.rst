Change log
==========

v2.2.1
-------

Features
^^^^^^^^

- Add **Information.is_readonly**- property to test, if a new value can be assigned to this information.

Fixes
^^^^^^

- return to default connection parameters from lib60870 (10s,15s,10s) #48, #51, #56
- increase default (connection) command timeout from 100ms to 10000ms to fit to connection parameters
- increase default (server) select timeout from 100ms to 10000ms to fit to connection parameters
- fix sending multiple messages for a batch, if maximum ASDU size exceeded #52
- fix server uses correct cause of transmission for interrogation response #53, #57
- fix server responds only with last station to general interrogation #54
- fix all periodic tasks (periodic monitoring reports, selection timeout) #52
- Connection interrogation command to global common address blocks until all stations send ACT_TERM, not just first station
- fix docstring for c104.ProtocolParameters timings: unit is seconds not milliseconds #48
- support try-except blocks in callbacks instead of catching all errors and removing the callback
- prevent invalid pointer access from lib60870 callbacks
- setting **Point.value** will raise ValueError if info is readonly
- setting **Point.quality** will raise ValueError if info is readonly

Dependencies
^^^^^^^^^^^^

- Add experimental support for python 3.13 free-threaded
- Minor update of lib60870 to latest
- Minor update of Mbed TLS to 3.6.3

v2.2.0
-------

Features
^^^^^^^^

- Add **Server.remove_station(common_address: int)** and **Connection.remove_station(common_address: int)**- methods to enable the removal of stations.
- Add **Station.remove_point(io_address: int)** method to allow the removal of points and facilitate reassigning the ``io_address`` to another point type.

v2.1.1
-------

Fixes
^^^^^^

- Fixed an issue where the source distribution erroneously included a ``__pycache__`` folder.

v2.1.0
-------

Features
^^^^^^^^

- Serve type hints for all classes and methods as a ``.pyi`` file.
- Add the property **Server.protocol_parameters** and **Connection.protocol_parameters** to enable reading and updating protocol parameters, such as window size and timeouts.
- Provide Batch Transmission support via the new **Batch** class, in combination with **Server.transmit_batch(...)**, for monitoring direction.
- Sending **Connection.counter_interrogation()** supports full Qualifier of Counter Interrogation (Rqt and Frz)
- Introduce the **Connection.on_unexpected_message()** callback to get informed about unsupported messages, or messages with type id conflicts
- Add the **Station.signal_initialized(cause=...)** method to support sending end-of-initialization messages per station to all connected clients.
- Introduce the **Client.on_station_initialized** callback to handle end-of-initialization messages.
- Send monitoring data formatted as sequences in periodic transmissions and interrogation responses, if possible.
- Add **TransportSecurity.set_ciphers()** to specify a list of supported cipher suites.
- Add **TransportSecurity.set_renegotiation_time()** to configure the TLS renegotiation time.
- Add **TransportSecurity.set_resumption_interval()** to define the session resumption interval.
- Raise a ``ValueError`` when modifying a **TransportSecurity** object that is already assigned to a client or server, as changes have no effect.

Dependencies
^^^^^^^^^^^^

- Add python 3.13 support
- Minor update of lib60870 to 2.3.4
- Major update of Mbed TLS to 3.6.2, add support for TLS 1.3

Fixes
^^^^^^

- Fix Client.get_connection method to accept ip and port or common_address argument
- Fix Qoc reference in docs
- Improved error message on assigning invalid information object to a points value or quality property

Deprecations
^^^^^^^^^^^^
- Property **number_of_object** renamed to **number_of_objects** in class **IncomingMessage**

Breaking Changes
^^^^^^^^^^^^^^^^
- Dropped TLS 1.1 support

v2.0.2
-------

Features
^^^^^^^^^

- Enhance the error messaging for invalid value and quality assignments of points

Fixes
^^^^^^

- Fix an issue with the point.value setter that was not functioning correctly for the following types: EventState, StartEvents, OutputCircuits, and PackedSingle
- Fix a segmentation fault that occurred during the string conversion of Quality and BinaryCounterQuality objects when unsupported bits were set

v2.0.1
-------

Fixes
^^^^^^

- Fix missing cyclic reports if debug flag c104.Debug.Server is not set
- Detect and handle invalid return values from callbacks
- Improve python docblock signatures
- Fix project URLs for pypi
- Fix documentation links in README

v2.0.0
-------

Features
^^^^^^^^^

- Add support for equipment protection points (*M_EP_TD_1*, *M_EP_TE_1*, *M_EP_TF_1*) and status with change detection (*M_PS_NA_1*)
- Add advanced property support for all messages
- Add point timer callback for extended event driven transmission scenarios
- Add option *c104.Init.MUTED*, to open a connection in muted state
- Add extended datetime.datetime support
- Add support for information object address **0**
- Improve command mode select and execute with automatic selection timeout
- Improve performance and stability
- Improve string representation for all classes
- Improve type safety

Breaking Changes
^^^^^^^^^^^^^^^^^

- Dropped python 3.6 support, since pybind11 does not support it any longer
- c104.Point signature changes (see below)
- c104.Station signature changes (see below)
- c104.Client signature changes (see below)
- c104.Connection signature changes (see below)
- c104.Server signature changes (see below)
- c104.IncomingMessage signature changes (see below)
- Renamed enum property **c104.Qoc.CONTINUOUS** to **c104.Qoc.PERSISTENT**. \
  This corresponds to the standard description for Qualifier of command.
- Removed deprecated function **c104.add_server(...)**, use ``c104.Server()`` constructor instead
- Removed deprecated function **c104.remove_server(...)**, remove last reference to server instance instead
- Removed deprecated function **c104.add_client(...)**, use ``c104.Client()`` constructor instead
- Removed deprecated function **c104.remove_client(...)**, remove last reference to client instance instead


Changed signatures in c104.Point
""""""""""""""""""""""""""""""""

The concept of a points value is not enough to support all properties of all protocol messages. Therefore, the value was replaced by individual information objects. Every point type has a specific information type that stores a specific value type but also other properties. This also ensures type safety because there is no automatic cast from a Python number to a required value class.

- Added property **point.info**
  This container class carries all protocol message specific properties of a point.

  .. code-block:: python

        single_point.info = c104.SingleInfo(True)
        double_point.info = c104.DoubleInfo(c104.Double.ON)
        step_point.info = c104.StepInfo(c104.Int5(13))
        binary_point.info = c104.BinaryInfo(c104.Byte32(12))
        normalized_point.info = c104.NormalizedInfo(c104.NormalizedFloat(-0.734))
        scaled_point.info = c104.ScaledInfo(c104.Int16(-24533))
        short_point.info = c104.ShortInfo(12.34)
        counter_point.info = c104.BinaryCounterInfo(345678)
        pe_event_point.info = c104.ProtectionEventInfo(c104.EventState.ON)
        pe_start_point.info = c104.ProtectionStartInfo(c104.StartEvents.PhaseL1 | c104.StartEvents.PhaseL2)
        pe_circuit_point.info = c104.ProtectionCircuitInfo(c104.OutputCircuits.PhaseL1)
        pe_changed_point.info = c104.StatusAndChanged(c104.PackedSingle.I0)

- Changed signature of **point.value** ``float`` **->** ``typing.Union[None, bool, c104.Double, c104.Step, c104.Int7, c104.Int16, int, c104.Byte32, c104.NormalizedFloat, float, c104.EventState, c104.StartEvents, c104.OutputCircuits, c104.PackedSingle]``
  The *point.value* property is a shortcut to *point.info.value* for convenience.
  Example: ``single_point.value = False``

- Removed property **point.value_uint32**
- Removed property **point.value_int32**
- Removed property **point.value_float**

- Changed signature of **point.quality** ``c104.Quality`` **->** ``typing.Union[None, c104.Quality, c104.BinaryCounterQuality]``
  The *point.quality* property is a shortcut to *point.info.quality* and returns point-specific types. For points without quality information, this will be None. Calling ``point.quality.is_good()`` can therefore result in an error if ``point.quality`` is **None**.

- Removed **point.set(...)** method
  Set a new info object ``point.info = ...`` instead, to update all properties like time and quality than just the value
  Example: ``cl_double_command.set(value=c104.Double.ON, timestamp_ms=1711111111111) -> cl_double_command.info = c104.DoubleCmd(state=c104.Double.ON, qualifier=c104.Qoc.LONG_PULSE, recorded_at=datetime.datetime.fromtimestamp(1711111111.111))``

- Changed **point.report_ms** setter validation
  The *report_ms* property must be a positive integer and a **multiple of the tick_rate_ms** of the corresponding server or client

- Removed property **point.updated_at_ms**: ``int``, use ``point.recorded_at`` instead
- Removed property **point.received_at_ms**: ``int``, use ``point.processed_at`` instead
- Removed property **point.sent_at_ms**: ``int``, use ``point.processed_at`` instead
- Removed property **point.reported_at_ms**: ``int``, use ``point.processed_at`` instead

- Added read-only property **point.recorded_at**: ``typing.Optional[datetime.datetime]``
  The timestamp sent with the info via protocol. At the sender side, this value will be set on info creation time and updated on info.value assigning. This timestamp will not be updated on point transmission. The property can be None, if the protocol message type does not contain a timestamp.
- Added read-only property **point.processed_at**: ``datetime.datetime``
  This timestamp stands for the last sending or receiving timestamp of this info.
- Added read-only property **point.selected_by**: ``typing.Optional[int]``
  If select this will be the originator address, otherwise None
- Changed signature of method **point.transmit** (cause: c104.Cot = c104.Cot.UNKNOWN_COT, qualifier: c104.Qoc = c104.Qoc.NONE) -> point.transmit(cause: c104.Cot)
  The qualifier is now part of the info object of command points and can be set via a new info assignment. The cause qualifier does not have a default value anymore so that this argument is obligatory now.
- Changed signature of **point.related_io_address** to accept None as value: ``int`` **->** ``typing.Optional[int]``
  This is necessary to accept a value of 0 as a valid io_address.
- Changed signature of **point.on_receive(...)** callback signature from ``(point: c104.Point, previous_state: dict, message: c104.IncomingMessage) -> c104.ResponseState`` to ``(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState`` \
  The argument ``previous_state: dict`` was replaced by argument ``previous_info: c104.Information``. Since all relevant is accessible via the info object, a dict is not required anymore. Instead, the previous info object will be provided.
- Added callback **point.on_timer(...)** \
  Callback signature function: ``(point: c104.Point) -> None`` \
  Register callback signature: ``point.on_timer(callable=on_timer, interval_ms=1000)`` \
  The *timer_ms* property must be a positive integer and a **multiple of the tick_rate_ms** of the corresponding server or client
- Added read-only property **point.interval_ms**: ``int`` \
  This property defines the interval between two on_timer callback executions. \
  This property can only be changed via the ``point.on_timer(...)`` method

Changed signatures in c104.Station
"""""""""""""""""""""""""""""""""""
- Changed signature of method **station.add_point(...)** \
  Parameter *io_address* accepts a value of ``0``. \
  Parameter *related_io_address*  accepts a value of ``0`` as valid IOA and a value of ``None`` as not set

Changed signatures in c104.IncomingMessage
"""""""""""""""""""""""""""""""""""""""""""
- Added read-only property info: Union[...]
- Removed property command_qualifier, use message.info.qualifier instead
- Removed property connection_string
- Removed property value
- Removed property quality

Changed signatures in c104.Client
""""""""""""""""""""""""""""""""""
- Changed signature of **constructor**
  Reduced default value of argument **command_timeout_ms** from ``1000ms`` to ``100ms``. \
  Reduced default value of argument **tick_rate_ms** from ``1000ms`` to ``100ms``. \
  The minimum tick rate is ``50ms``.
- Added read-only property **client.tick_rate_ms**: ``int``

Changed signatures in c104.Connection
""""""""""""""""""""""""""""""""""""""
- Added read-only property **connection.connected_at**: ``typing.Optional[datetime.datetime]``
- Added read-only property **connection.disconnected_at**: ``typing.Optional[datetime.datetime]``
- Add c104.Init.MUTED to connect to a server without activating the message transmission.
- Removed c104.ConnectionState values: OPEN_AWAIT_UNMUTE, OPEN_AWAIT_INTERROGATION, OPEN_AWAIT_CLOCK_SYNC
  The connection will change from CLOSED_AWAIT_OPEN to OPEN_MUTED, will then execute the init commands, if any and change the state afterwards to OPEN if init != c104.Init.MUTED. The intermediary states are not required anymore.
- Instead of using to wait for a connection establishment:
  while not connection.is_connected:
  time.sleep(1)
  wait for state open so that not only connection is established but also init commands are finished
  while connection.state != c104.ConnectionState.OPEN:
  time.sleep(1)

Changed signatures in c104.Server
""""""""""""""""""""""""""""""""""
- Changed signature of **constructor** \
  Add argument **select_timeout_ms** to constructor with default value ``100ms`` \
  Reduced default value of **tick_rate_ms** from ``1000ms`` to ``100ms``. \
  The minimum tick rate is 50ms.
- Added read-only property **client.tick_rate_ms**: ``int``

Bugfixes
^^^^^^^^^^
- Read property **IncomingMessage.raw** caused SIGABRT
- **Server.active_connection_count** counts also inactive open connections
- fix select detection in **c104.explain_bytes_dict(...)**
- **point.transmit(...)** throws an exception if the same point is in an active transmission
- auto set environment variable **PYTHONUNBUFFERED** to avoid delayed print output from Python callbacks

v1.18
-------
- Add support for Qualifier of Command for single, double and regulating step commands
- Fix transmit updated_at timestamp for time aware point
- c104.Point.set method signature improved (non-breaking):
  - Add keyword argument timestamp_ms to allow setting a points value in combination with an updated_at_ms timestamp
  - Improve value argument to support instances of type c104.Double and c104.Step as setter for c104.Point.value does
- Improve GIL handling for methods station.add_point, server.stop and client.stop

v1.17
-------
- Fix (1.17.1): Fix select-and-execute for C_SE_NA
- Fix (1.17.1): Fix armv7 build

- Add optional feature **Select-And-Execute** (also called Select-Before-Execute)
  - Add enum c104.CommandMode
  - Add properties point.command_mode, point.selected_by and incomingmessage.is_select_command
  -  on_receive callback argument previous_state contains key selected_by
  - Add select field to explain_bytes and explain_bytes_dict

- Fix free command response state key if command was never send
- Improve point transmission handling
- Improve documentation

v1.16
-------
- Add feature TLS (working versions: SSLv3.0, TLSv1.0, TLSv1.1, TLSv1.2; not working: TLSv1.3)
- Fix potential segmentation fault by using smart pointer with synchronized reference counter between cpp and python
- Improve CMake structure
- Improve reconnect behaviour
- Update lib60870-C to latest

v1.15
-------
- Fix (1.15.2): Fix deadlock between GIL and client-internal mutex.
- Add new Connection callback **on_state_change** (connection: c104.Connection, state: c104.ConnectionState) -> None
- Add new enum c104.ConnectionState (OPEN, CLOSED, ...)
- Allow COT 7,9,10 for command point transmit() from server side to support manual/lazy command responses
- Add new enum c104.ResponseState (FAILURE, SUCCESS, NONE)
- **BC signature of callback server.on_clock_sync changed**
    - Return c104.ResponseState instead of bool
- **BC signature of callback point.on_receive changed**
    - Return c104.ResponseState instead of bool

v1.14
-------
- Fix (1.14.2): Fix potential segmentation fault
- Fix (1.14.1): Add missing option c104.Init.NONE
- Add c104.Init enum to configure outgoing commands after START_DT, defaults to c104.Init.ALL which is equal to previous behaviour
- Clients timeout_ms parameter is used to configure maximum rtt for message in lib60870-C \
  (APCI Parameter t1: max(1, (int)round(timeout_ms/1000)))
- **BC callback signature validation**
    - Allow functools.partial, functools.partialmethod and extra arguments in callbacks that have a default/bound value
    - Ignore arguments with non-empty default value n callback signature validation

v1.13
-------
- Fix (1.13.6): try send clock sync only once after start_dt
- Fix (1.13.5): Silence debug output, update dependencies
- Fix (1.13.4): PointCommand encode REGULATION STEP COMMAND values, windows stack manipulation in server
- Fix (1.13.3): IncomingMessage decode DOUBLE POINT values 0.0, 1.0, 2.0, 3.0
- Fix (1.13.3): IncomingMessage allows 0.0,1.0,2.0,3.0 values for DoubleCommands, message.value returns value instead of IOA
- Fix (1.13.2): Server sends multiple ASDU per TypeID in InterrogationResponse or Periodic transmission if IOs exceed single ASDU size
- **BC for on_clock_sync** \
  Callable must return a bool to provide act-con feedback to client
- **Respond to global CA messages** \
  Fix: Server confirms messages that are addressed at global ca from each local CA with its own address.

v1.12
-------
- **Replace BitSets by Enum flags** \
  Change usage of Debug and Quality attributes
- **Start periodic transmission instantly** after receiving START_DT, do not wait for a first interrogation command

v1.11
-------
- **Add python 3.6 support**
- **Add Windows support**
- **Migrated from boost::python to pybind11** \
  Drop all dependencies to boost libraried and replace bindings by header only template library pybind11.
- **Simplified build process via setuptools and cmake** \
  Integrate lib60870 into cmake to build everything in a single build process.
- **Improve callback handling**
    - *Function:* A **reference** is stored internally with valid reference counter.
    - *Method:* A **reference** to the bounded method is stored internally.

v1.10
-------
- **Add ARM support**
- **New DebugFlag: GIL** \
  Print debug information when GIL is acquired or released.
- **New coding convention for callbacks:**
    - Callback function signature must match perfectly (variable names, order, return and type hints).
    - *Lambda:* Usage of lambda function is **not possible** as type hinting information are not added to the function object itself, only to the namespace the object is stored in.
    - *Function:* A **copy** (type.FunctionType) is stored internally using the same references as the original function to guarantee function existence. (^1.10.2)
    - *Method:* A **reference** to the object is stored internally with the name of the method. (^1.10.2)

v1.9
-------
- **New coding convention:** Caller passes self-reference as first argument to callback functions.
    - Client.on_new_station: Client reference as additional argument in the first place
    - Client.on_new_point: Client reference as additional argument in the first place
    - Connection.on_receive_raw: Connection reference as additional argument in the first place
    - Connection.on_send_raw: Connection reference as additional argument in the first place
    - Server.on_receive_raw: Server reference as additional argument in the first place
    - Server.on_send_raw: Server reference as additional argument in the first place
    - Server.on_connect: Server reference as additional argument in the first place
    - Server.on_clock_sync: Server reference as additional argument in the first place
    - Server.on_unexpected_message: Server reference as additional argument in the first place
