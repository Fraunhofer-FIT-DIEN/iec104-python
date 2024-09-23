# Change log

## v2.0
### Fixes
- Fix (2.0.1): Fix missing cyclic reports if debug flag c104.Debug.Server is not set #28
- Fix (2.0.1): Improve python docblock signatures
- Fix (2.0.1): Fix project URLs for pypi
- Fix (2.0.1): Fix documentation links in README
### Features
- Support for equipment protection points (*M_EP_TD_1*, *M_EP_TE_1*, *M_EP_TF_1*) and status with change detection (*M_PS_NA_1*)
- Command mode select and execute with automatic selection timeout
- Added point timer callback for extended event driven transmission scenarios
- Added Option *MUTED* to *c104.Init*, to open a connection in muted state
- Extended datetime.datetime support
- Performance and stability improvements
- Improved string representation for all classes
- Improved type safety
### Breaking Changes
- Dropped python 3.6 support, since pybind11 does not suppor
#### c104.Point
The concept of a points value is not enough to support all properties of all protocol messages. therefore the value was replaced by individual information objects. Every point type has a specific information type that stores a specific value type but also other properties.
This also ensures type safety, because there is no automatic cast from a Python number to a required value class.
- Added property **point.info** \
  This container class carry's all protocol message specific properties of a point.
  ```python
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
  ```
- Changed signature of **point.value** `float` **->** `Union[None,bool,c104.Double,c104.Step,c104.Int7,c104.Int16,int,c104.Byte32,c104.NormalizedFloat,float,c104.EventState,c104.StartEvents,c104.OutputCircuits,c104.PackedSingle]` \
  The `point.value` property is a shortcut to `point.info.value` for convenience. \
  Example: `single_point.value = False`
- Removed property **point.value_uint32**
- Removed property **point.value_int32**
- Removed property **point.value_float**
- Changed signature of **point.quality** `c104.Quality` **->** `Union[None,c104.Quality,c104.BinaryCounterQuality]`
  The `point.quality` property is a shortcut to `point.info.quality` and returns point specific types. For points without quality information this will be None. Calling `point.quality.is_good()` can therefore result in an error, if `point.quality` is **None**.
- Removed **point.set(...)** method \
  Set a new info object `point.info = ...` instead, to update all properties like time and quality than just the value \
  Example: `cl_double_command.set(value=c104.Double.ON, timestamp_ms=1711111111111) -> cl_double_command.info = c104.DoubleCmd(state=c104.Double.ON, qualifier=c104.Qoc.LONG_PULSE, recorded_at=datetime.datetime.fromtimestamp(1711111111.111))`
- Changed **point.report_ms** setter validation
  The `report_ms` property must be a positive integer and a **multiple of the tick_rate_ms** of the corresponding server or client
- Removed property **point.updated_at_ms**: `int`, use `point.recorded_at` instead
- Removed property **point.received_at_ms**: `int`, use `point.processed_at` instead
- Removed property **point.sent_at_ms**: `int`, use `point.processed_at` instead
- Removed property **point.reported_at_ms**: `int`, use `point.processed_at` instead
- Added read-only property **point.recorded_at**: `Optional[datetime.datetime]`
  The timestamp send with the info via protocol. At the sender side this value will be set on info creation time and updated on info.value assigning. This timestamp will not be updated on point transmission. The property can be None, if the protocol message type does not contain a timestamp.
- Added read-only property **point.processed_at**: `datetime.datetime`
  This timestamp stands for the last sending or receiving timestamp of this info.
- Added read-only property **point.selected_by**: `Optional[int]`
  If select this will be the originator address, otherwise None
- Changed signature of method **point.transmit**(cause: c104.Cot = c104.Cot.UNKNOWN_COT, qualifier: c104.Qoc = c104.Qoc.NONE) -> point.transmit(cause: c104.Cot)
  The qualifier is now part of the info object of command points and can be set via a new info assignment. The cause qualifier does not have a default value anymore so that this argument is obligatory now.
- Changed signature of **point.related_io_address** to accept None as value: `int` **->** `Optional[int]`
  This is necessary to accept a value of 0 as a valid io_address.
- Changed signature of **point.on_receive(...)** callback signature from `(point: c104.Point, previous_state: dict, message: c104.IncomingMessage) -> c104.ResponseState` to `(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState` \
  The argument `previous_state: dict` was replaced by argument `previous_info: c104.Information`. Since all relevant is accessible via the info object, a dict is not required anymore. Instead, the previous info object will be provided.
- Added callback **point.on_timer(...)** \
  Callback signature function: `(point: c104.Point) -> None` \
  Register callback signature: `point.on_timer(callable=on_timer, interval_ms=1000)` \
  The `timer_ms` property must be a positive integer and a **multiple of the tick_rate_ms** of the corresponding server or client
- Added read-only property **point.interval_ms**: `int` \
  This property defines the interval between two on_timer callback executions. \
  This property can only be changed via the `point.on_timer(...)` method

#### c104.Station
- Changed signature of method **station.add_point(...)** \
  Parameter `io_address` accepts a value of `0`. \
  Parameter `related_io_address`  accepts a value of `0` as valid IOA and a value of `None` as not set

##### c104.IncomingMessage
- Added read-only property info: Union[...]
- Removed property command_qualifier, use message.info.qualifier instead
- Removed property connection_string
- Removed property value
- Removed property quality

#### c104.Client
- Changed signature of **constructor**
  Reduced default value of argument **command_timeout_ms** from `1000ms` to `100ms`. \
  Reduced default value of argument **tick_rate_ms** from `1000ms` to `100ms`. \
  The minimum tick rate is `50ms`.
- Added read-only property **client.tick_rate_ms**: `int`

#### Connection
- Added read-only property **connection.connected_at**: `Optional[datetime.datetime]`
- Added read-only property **connection.disconnected_at**: `Optional[datetime.datetime]`
- Add c104.Init.MUTED to connect to a server without activating the message transmission.
- Removed c104.ConnectionState values: OPEN_AWAIT_UNMUTE, OPEN_AWAIT_INTERROGATION, OPEN_AWAIT_CLOCK_SYNC
  The connection will change from CLOSED_AWAIT_OPEN to OPEN_MUTED, will then execute the init commands, if any and change the state afterwards to OPEN if init != c104.Init.MUTED. The intermediary states are not required anymore.
- Instead of using to wait for a connection establishment:
  while not connection.is_connected:
  time.sleep(1)
  wait for state open so that not only connection is established but also init commands are finished
  while connection.state != c104.ConnectionState.OPEN:
  time.sleep(1)

#### c104.Server
- Changed signature of **constructor** \
  Add argument **select_timeout_ms** to constructor with default value `100ms` \
  Reduced default value of **tick_rate_ms** from `1000ms` to `100ms`. \
  The minimum tick rate is 50ms.
- Added read-only property **client.tick_rate_ms**: `int`

#### c104 Enums, Numbers and Helper Classes
- Renamed enum property **c104.Qoc.CONTINUOUS** to **c104.Qoc.PERSISTENT** \
  This corresponds to the standard description for Qualifier of command.

#### c104 Global functions
- Removed deprecated function **c104.add_server(...)**, use `c104.Server()` constructor instead
- Removed deprecated function **c104.remove_server(...)**, remove last reference to server instance instead
- Removed deprecated function **c104.add_client(...)**, use `c104.Client()` constructor instead
- Removed deprecated function **c104.remove_client(...)**, remove last reference to client instance instead

### Bugfixes
- Accept **io_address=0** for points
- Read property **IncomingMessage.raw** caused SIGABRT
- **Server.active_connection_count** counts also inactive open connections
- fix select detection in **c104.explain_bytes_dict(...)**
- **point.transmit(...)** throws an exception if the same point is in an active transmission
- auto set environment variable **PYTHONUNBUFFERED** to avoid delayed print output from Python callbacks

### Dependencies
- Update catch2 to 3.7.0
- Update pybind11 to 2.13.5
- Update lib60870-C to latest (>2.3.3)
- Update mbedtls to 2.28.8

- Todo
- Remove outdated documentation
- Removed message.requires_confirmation ?
- dp_related_ioa call .value() in ctor point , use setter
- Common address 0?
- Timer_ms multiple int of tick rate?

## v1.18
- Add support for Qualifier of Command for single, double and regulating step commands
- Fix transmit updated_at timestamp for time aware point
- c104.Point.set method signature improved (non-breaking):
  - Add keyword argument timestamp_ms to allow setting a points value in combination with an updated_at_ms timestamp
  - Improve value argument to support instances of type c104.Double and c104.Step as setter for c104.Point.value does
- Improve GIL handling for methods station.add_point, server.stop and client.stop

## v1.17
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

## v1.16

- Add feature TLS (working versions: SSLv3.0, TLSv1.0, TLSv1.1, TLSv1.2; not working: TLSv1.3)

- Fix potential segmentation fault by using smart pointer with synchronized reference counter between cpp and python

- Improve CMake structure

- Improve reconnect behaviour

- Update lib60870-C to latest

## v1.15
- Fix (1.15.2): Fix deadlock between GIL and client-internal mutex.

- Add new Connection callback __on_state_change__ (connection: c104.Connection, state: c104.ConnectionState) -> None

- Add new enum c104.ConnectionState (OPEN, CLOSED, ...)

- Allow COT 7,9,10 for command point transmit() from server side to support manual/lazy command responses

- Add new enum c104.ResponseState (FAILURE, SUCCESS, NONE)

- __BC signature of callback server.on_clock_sync changed__
    - Return c104.ResponseState instead of bool

- __BC signature of callback point.on_receive changed__
    - Return c104.ResponseState instead of bool

## v1.14
- Fix (1.14.2): Fix potential segmentation fault

- Fix (1.14.1): Add missing option c104.Init.NONE

- Add c104.Init enum to configure outgoing commands after START_DT, defaults to c104.Init.ALL which is equal to previous behaviour

- Clients timeout_ms parameter is used to configure maximum rtt for message in lib60870-C \
  (APCI Parameter t1: max(1, (int)round(timeout_ms/1000)))

- __BC callback signature validation__
    - Allow functools.partial, functools.partialmethod and extra arguments in callbacks that have a default/bound value
    - Ignore arguments with non-empty default value n callback signature validation


## v1.13
- Fix (1.13.6): try send clock sync only once after start_dt

- Fix (1.13.5): Silence debug output, update dependencies

- Fix (1.13.4): PointCommand encode REGULATION STEP COMMAND values, windows stack manipulation in server

- Fix (1.13.3): IncomingMessage decode DOUBLE POINT values 0.0, 1.0, 2.0, 3.0

- Fix (1.13.3): IncomingMessage allows 0.0,1.0,2.0,3.0 values for DoubleCommands, message.value returns value instead of IOA

- Fix (1.13.2): Server sends multiple ASDU per TypeID in InterrogationResponse or Periodic transmission if IOs exceed single ASDU size

- __BC for on_clock_sync__ \
  Callable must return a bool to provide act-con feedback to client
- __Respond to global CA messages__ \
  Fix: Server confirms messages that are addressed at global ca from each local CA with its own address.

## v1.12
- __Replace BitSets by Enum flags__ \
  Change usage of Debug and Quality attributes
- __Start periodic transmission instantly__ after receiving START_DT, do not wait for a first interrogation command


## v1.11
- __Add python 3.6 support__

- __Add Windows support__

- __Migrated from boost::python to pybind11__ \
  Drop all dependencies to boost libraried and replace bindings by header only template library pybind11.
- __Simplified build process via setuptools and cmake__ \
  Integrate lib60870 into cmake to build everything in a single build process.
- __Improve callback handling__
    - _Function:_ A __reference__ is stored internally with valid reference counter.
    - _Method:_ A __reference__ to the bounded method is stored internally.

## v1.10
- __Add ARM support__

- __New DebugFlag: GIL__ \
  Print debug information when GIL is acquired or released.
- __New coding convention for callbacks:__
    - Callback function signature must match perfectly (variable names, order, return and type hints).
    - _Lambda:_ Usage of lambda function is __not possible__ as type hinting information are not added to the function object itself, only to the namespace the object is stored in.
    - _Function:_ A __copy__ (type.FunctionType) is stored internally using the same references as the original function to guarantee function existence. (^1.10.2)
    - _Method:_ A __reference__ to the object is stored internally with the name of the method. (^1.10.2)

## v1.9
__New coding convention:__ Caller passes self-reference as first argument to callback functions.

- Client.on_new_station: Client reference as additional argument in the first place
- Client.on_new_point: Client reference as additional argument in the first place
- Connection.on_receive_raw: Connection reference as additional argument in the first place
- Connection.on_send_raw: Connection reference as additional argument in the first place
- Server.on_receive_raw: Server reference as additional argument in the first place
- Server.on_send_raw: Server reference as additional argument in the first place
- Server.on_connect: Server reference as additional argument in the first place
- Server.on_clock_sync: Server reference as additional argument in the first place
- Server.on_unexpected_message: Server reference as additional argument in the first place
