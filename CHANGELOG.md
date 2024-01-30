# Change log

## v1.17

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
- Server.on_clock_sync: Server reference as additional argument in the first placele
- Server.on_unexpected_message: Server reference as additional argument in the first place
