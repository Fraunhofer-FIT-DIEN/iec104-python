from __future__ import annotations
import collections.abc
import datetime
import typing
__all__ = ['Batch', 'BinaryCmd', 'BinaryCounterInfo', 'BinaryCounterQuality', 'BinaryInfo', 'Byte32', 'Client', 'Coi', 'CommandMode', 'Connection', 'ConnectionState', 'Cot', 'Debug', 'Double', 'DoubleCmd', 'DoubleInfo', 'EventState', 'Frz', 'IncomingMessage', 'Information', 'Init', 'Int16', 'Int7', 'NormalizedCmd', 'NormalizedFloat', 'NormalizedInfo', 'OutputCircuits', 'PackedSingle', 'Point', 'ProtectionCircuitInfo', 'ProtectionEventInfo', 'ProtectionStartInfo', 'ProtocolParameters', 'Qoc', 'Qoi', 'Quality', 'ResponseState', 'Rqt', 'ScaledCmd', 'ScaledInfo', 'Server', 'ShortCmd', 'ShortInfo', 'SingleCmd', 'SingleInfo', 'StartEvents', 'Station', 'StatusAndChanged', 'Step', 'StepCmd', 'StepInfo', 'TlsCipher', 'TlsVersion', 'TransportSecurity', 'Type', 'UInt16', 'UInt5', 'UInt7', 'Umc', 'disable_debug', 'enable_debug', 'explain_bytes', 'explain_bytes_dict', 'get_debug_mode', 'set_debug_mode']
class Batch:
    """
    This class represents a batch of outgoing monitoring messages of the same station and type
    """
    def __init__(self, cause: Cot, points: list[Point] | None = None) -> None:
        """
        create a new batch of monitoring messages of the same station and the same type

        Parameters
        ----------
        cause: c104.Cot
            what caused the transmission of the monitoring data
        points: list[c104.Point], optional
            initial list of points

        Raises
        ------
        ValueError
            if one point in the list is not compatible with the others

        Example
        -------
        >>> batch = c104.Batch(cause=c104.Cot.SPONTANEOUS, points=[point1, point2, point3])
        """
    def add_point(self, point: Point) -> None:
        """
        add a new point to this Batch

        Parameters
        ----------
        point: c104.Point
            to be added point

        Returns
        -------
        None

        Raises
        ------
        ValueError
            if point is not compatible with the batch or if it is already in the batch

        Example
        -------
        >>> my_batch.add_point(my_point)
        """
    @property
    def common_address(self) -> int:
        """
        common address (1-65534)
        """
    @property
    def cot(self) -> Cot:
        """
        cause of transmission
        """
    @property
    def has_points(self) -> bool:
        """
        test if batch contains points
        """
    @property
    def is_negative(self) -> bool:
        """
        test if negative flag is set
        """
    @property
    def is_sequence(self) -> bool:
        """
        test if sequence flag is set
        """
    @property
    def is_test(self) -> bool:
        """
        test if test flag is set
        """
    @property
    def number_of_objects(self) -> int:
        """
        represents the number of information objects contained in this message
        """
    @property
    def originator_address(self) -> int:
        """
        originator address (0-255)
        """
    @property
    def points(self) -> list[Point]:
        """
        get a list of contained points
        """
    @property
    def type(self) -> Type:
        """
        IEC60870 message type identifier
        """
class BinaryCmd(Information):
    """
    This class represents all specific binary command information
    """
    def __init__(self, blob: Byte32, recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new binary command

        Parameters
        ----------
        blob: c104.Byte32
            Binary command value
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> binary_cmd = c104.BinaryCmd(blob=c104.Byte32(1234), recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def blob(self) -> Byte32:
        """
        the value
        """
    @property
    def quality(self) -> None:
        """
        This information does not contain quality information.
        """
    @property
    def value(self) -> Byte32:
        """
        references property ``blob``

        The setter is available via point.value=xyz
        """
class BinaryCounterInfo(Information):
    """
    This class represents all specific integrated totals of binary counter point information
    """
    def __init__(self, counter: int, sequence: UInt5, quality: BinaryCounterQuality = BinaryCounterQuality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new short measurement info

        Parameters
        ----------
        counter: int
            Counter value
        sequence: c104.UInt5
            Counter info sequence number
        quality: c104.BinaryCounterQuality
            Binary counter quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> counter_info = c104.BinaryCounterInfo(counter=2345, sequence=c104.UInt5(35), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def counter(self) -> int:
        """
        the value
        """
    @property
    def quality(self) -> BinaryCounterQuality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def sequence(self) -> UInt5:
        """
        the counter sequence number
        """
    @property
    def value(self) -> int:
        """
        references property ``counter``

        The setter is available via point.value=xyz
        """
class BinaryCounterQuality:
    """
    This enum contains all binary counter quality issue bits to interpret and manipulate counter quality.
    """
    Adjusted: typing.ClassVar[BinaryCounterQuality]
    Carry: typing.ClassVar[BinaryCounterQuality]
    Invalid: typing.ClassVar[BinaryCounterQuality]
    __members__: typing.ClassVar[dict[str, BinaryCounterQuality]]

    def is_any(self) -> bool:
        """
        test if there are any bits set
        """
    def is_good(self) -> bool:
        """
        test if no quality problems are set
        """
    @property
    def value(self) -> int:
        """
        combined bits in integer representation
        """
class BinaryInfo(Information):
    """
    This class represents all specific binary point information
    """
    def __init__(self, blob: Byte32, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new binary info

        Parameters
        ----------
        blob: c104.Byte32
            Binary status value
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> binary_info = c104.BinaryInfo(blob=c104.Byte32(2345), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def blob(self) -> Byte32:
        """
        the value
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def value(self) -> Byte32:
        """
        references property ``blob``

        The setter is available via point.value=xyz
        """
class Byte32:
    """
    This object is compatible to native bytes and ensures that the length of the bytes is exactly 32 bit.
    """
    def __bytes__(self) -> bytes:
        """
        convert to native bytes
        """
    def __init__(self, value: bytes | int) -> None:
        """
        create a new fixed-length bytes representation

        Parameters
        ----------
        value: typing.Union[bytes, int]
            native byte data

        Raises
        ------
        OverflowError
            cannot convert value into 4 bytes representation

        Example
        -------
        >>> fixed_byte32 = c104.Byte32(0b10101010111)
        """
class Client:
    """
    This class represents a local client and provides access to meta information and connected remote servers
    """
    def __init__(self, tick_rate_ms: int = 100, command_timeout_ms: int = 100, transport_security: TransportSecurity | None = None) -> None:
        """
        create a new 104er client

        Parameters
        ----------
        tick_rate_ms : int
            client thread update interval
        command_timeout_ms : int
            time to wait for a command response
        transport_security : c104.TransportSecurity, optional
            TLS configuration object

        Example
        -------
        >>> my_client = c104.Client(tick_rate_ms=100, command_timeout_ms=100)
        """
    def add_connection(self, ip: str, port: int = 2404, init: Init | None = Init.ALL) -> Connection | None:
        """
        add a new remote server connection to this client and return the new connection object

        Parameters
        ----------
        ip: str
            remote terminal units ip address
        port: int
            remote terminal units port
        init: c104.Init
            communication initiation commands

        Returns
        -------
        c104.Connection, optional
            connection object, if added, else None

        Raises
        ------
        ValueError
            ip or port are invalid

        Example
        -------
        >>> con = my_client.add_connection(ip="192.168.50.3", port=2406, init=c104.Init.ALL)
        """
    def disconnect_all(self) -> None:
        """
        close all connections

        Example
        -------
        >>> my_client.disconnect_all()
        """
    def get_connection(self, ip: str = "", port: int = 2404, common_address: int = 0) -> Connection | None:
        """
        get a connection (either by ip and port or by common_address)

        Parameters
        ----------
        ip: str, optional
            remote terminal units ip address
        port: int, optional
            remote terminal units port
        common_address: int, optional
            common address (value between 1 and 65534)

        Returns
        -------
        c104.Connection, optional
            connection object, if found else None

        Example
        -------
        >>> con = my_client.get_connection(ip="192.168.50.3", port=2406)
        >>> con = my_client.get_connection(common_address=4711)
        """
    def on_new_point(self, callable: collections.abc.Callable[[Client, Station, int, Type], None]) -> None:
        """
        set python callback that will be executed on incoming message from unknown point

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Client, c104.Station, int, c104.Type], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        client: c104.Client
            client instance
        station: c104.Station
            station reporting point
        io_address: int
            point information object address (value between 0 and 16777215)
        point_type: c104.Type
            point information type

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def cl_on_new_point(client: c104.Client, station: c104.Station, io_address: int, point_type: c104.Type) -> None:
        >>>     print("NEW POINT: {1} with IOA {0} | CLIENT OA {2}".format(io_address, point_type, client.originator_address))
        >>>     point = station.add_point(io_address=io_address, type=point_type)
        >>>
        >>> my_client.on_new_point(callable=cl_on_new_point)
        """
    def on_new_station(self, callable: collections.abc.Callable[[Client, Connection, int], None]) -> None:
        """
        set python callback that will be executed on incoming message from unknown station

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Client, c104.Connection, int], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        --------------------
        client: c104.Client
            client instance
        connection: c104.Connection
            connection reporting station
        common_address: int
            station common address (value between 1 and 65534)

        Callable Returns
        -----------------
        None

        Example
        -------
        >>> def cl_on_new_station(client: c104.Client, connection: c104.Connection, common_address: int) -> None:
        >>>     print("NEW STATION {0} | CLIENT OA {1}".format(common_address, client.originator_address))
        >>>     connection.add_station(common_address=common_address)
        >>>
        >>> my_client.on_new_station(callable=cl_on_new_station)
        """
    def on_station_initialized(self, callable: collections.abc.Callable[[Client, Station, Coi], None]) -> None:
        """
        set python callback that will be executed on incoming end of initialization message from stations

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Client, c104.Station, c104.Coi], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        --------------------
        client: c104.Client
            client instance
        station: c104.Station
            reporting station
        cause: c104.Coi
            what caused the (re-)initialization procedure

        Callable Returns
        -----------------
        None

        Example
        -------
        >>> def cl_on_station_initialized(client: c104.Client, station: c104.Station, cause: c104.Coi) -> None:
        >>>     print("STATION {0} INITIALIZED due to {1} | CLIENT OA {2}".format(station.common_address, cause, client.originator_address))
        >>>
        >>> my_client.on_station_initialized(callable=cl_on_station_initialized)
        """
    def reconnect_all(self) -> None:
        """
        close and reopen all connections

        Example
        -------
        >>> my_client.reconnect_all()
        """
    def start(self) -> None:
        """
        start client and connect all connections

        Example
        -------
        >>> my_client.start()
        """
    def stop(self) -> None:
        """
        disconnect all connections and stop client

        Example
        -------
        >>> my_client.stop()
        """
    @property
    def active_connection_count(self) -> int:
        """
        get number of active (open and not muted) connections to servers
        """
    @property
    def connections(self) -> list[Connection]:
        """
        list of all remote terminal unit (server) Connection objects
        """
    @property
    def has_active_connections(self) -> bool:
        """
        test if client has active (open and not muted) connections to servers
        """
    @property
    def has_connections(self) -> bool:
        """
        test if client has at least one remote server connection
        """
    @property
    def has_open_connections(self) -> bool:
        """
        test if client has open connections to servers
        """
    @property
    def is_running(self) -> bool:
        """
        test if client is running
        """
    @property
    def open_connection_count(self) -> int:
        """
        represents the number of open connections to servers
        """
    @property
    def originator_address(self) -> int:
        """
        originator address of this client (0-255)
        """
    @originator_address.setter
    def originator_address(self, value: int) -> None:
        """
        assign the originator address of this client (0-255)

        Parameters
        ----------
        value: int
            new originator address of this client (0-255)

        Returns
        -------
        None

        Raises
        ------
        ValueError
            not a valid originator address
        """
    @property
    def tick_rate_ms(self) -> int:
        """
        the clients tick rate in milliseconds
        """
class Coi:
    """
    This enum contains all valid IEC60870 cause of initialization values.
    """
    LOCAL_MANUAL_RESET: typing.ClassVar[Coi]
    LOCAL_POWER_ON: typing.ClassVar[Coi]
    REMOTE_RESET: typing.ClassVar[Coi]
    __members__: typing.ClassVar[dict[str, Coi]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class CommandMode:
    """
    This enum contains all command transmission modes a clientmay use to send commands.
    """
    DIRECT: typing.ClassVar[CommandMode]
    SELECT_AND_EXECUTE: typing.ClassVar[CommandMode]
    __members__: typing.ClassVar[dict[str, CommandMode]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Connection:
    """
    This class represents connections from a client to a remote server and provides access to meta information and containing stations
    """
    def add_station(self, common_address: int) -> Station | None:
        """
        add a new station to this connection and return the new station object

        Parameters
        ----------
        common_address: int
            station common address (value between 1 and 65534)

        Returns
        -------
        c104.Station, optional
            station object, if station was added, else None

        Example
        -------
        >>> station = my_connection.add_station(common_address=15)
        """
    def clock_sync(self, common_address: int, wait_for_response: bool = True) -> bool:
        """
        send a clock synchronization command to the remote terminal unit (server)
        the clients OS time is used

        Parameters
        ----------
        common_address: int
            station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
        wait_for_response: bool
            block call until command success or failure response received?

        Returns
        -------
        bool
            True, if connection is Open, False otherwise

        Example
        -------
        >>> if not my_connection.clock_sync(common_address=47):
        >>>     raise ValueError("Cannot send clock sync command")
        """
    def connect(self) -> None:
        """
        initiate connection to remote terminal unit (server) in a background thread (non-blocking)

        Example
        -------
        >>> my_connection.connect()
        """
    def counter_interrogation(self, common_address: int, cause: Cot = Cot.ACTIVATION, qualifier: Rqt = Rqt.GENERAL, freeze: Frz = Frz.READ, wait_for_response: bool = True) -> bool:
        """
        send a counter interrogation command to the remote terminal unit (server)

        Parameters
        ----------
        common_address: int
            station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
        cause: c104.Cot
            cause of transmission
        qualifier: c104.Rqt
            what counters are addressed
        freeze: c104.Frz
            counter behaviour
        wait_for_response: bool
            block call until command success or failure response received?

        Returns
        -------
        bool
            True, if connection is Open, False otherwise

        Example
        -------
        >>> if not my_connection.counter_interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Rqt.GENERAL, freeze=c104.Frz.COUNTER_RESET):
        >>>     raise ValueError("Cannot send counter interrogation command")
        """
    def disconnect(self) -> None:
        """
        close connection to remote terminal unit (server)

        Example
        -------
        >>> my_connection.disconnect()
        """
    def get_station(self, common_address: int) -> Station | None:
        """
        get a station object via common address

        Parameters
        ----------
        common_address: int
            station common address (value between 1 and 65534)

        Returns
        -------
        c104.Station, optional
            station object, if found, else None

        Example
        -------
        >>> station_14 = my_connection.get_station(common_address=14)
        """
    def interrogation(self, common_address: int, cause: Cot = Cot.ACTIVATION, qualifier: Qoi = Qoi.STATION, wait_for_response: bool = True) -> bool:
        """
        send an interrogation command to the remote terminal unit (server)

        Parameters
        ----------
        common_address: int
            station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
        cause: c104.Cot
            cause of transmission
        qualifier: c104.Qoi
            qualifier of interrogation
        wait_for_response: bool
            block call until command success or failure response received?

        Returns
        -------
        bool
            True, if connection is Open, False otherwise

        Raises
        ------
        ValueError
            qualifier is invalid

        Example
        -------
        >>> if not my_connection.interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Qoi.STATION):
        >>>     raise ValueError("Cannot send interrogation command")
        """
    def mute(self) -> bool:
        """
        tell the remote terminal unit (server) that this connection is muted, prohibit monitoring messages

        Returns
        -------
        bool
            True, if connection is Open, False otherwise

        Example
        -------
        >>> if not my_connection.mute():
        >>>     raise ValueError("Cannot mute connection")
        """
    def on_receive_raw(self, callable: collections.abc.Callable[[Connection, bytes], None]) -> None:
        """
        set python callback that will be executed on incoming message

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Connection, bytes], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        connection: c104.Connection
            connection instance
        data: bytes
            raw message bytes

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def con_on_receive_raw(connection: c104.Connection, data: bytes) -> None:
        >>>     print("-->| {1} [{0}] | CON {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), connection.ip, connection.port))
        >>>
        >>> my_connection.on_receive_raw(callable=con_on_receive_raw)
        """
    def on_send_raw(self, callable: collections.abc.Callable[[Connection, bytes], None]) -> None:
        """
        set python callback that will be executed on outgoing message

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Connection, bytes], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        connection: c104.Connection
            connection instance
        data: bytes
            raw message bytes

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def con_on_send_raw(connection: c104.Connection, data: bytes) -> None:
        >>>     print("<--| {1} [{0}] | CON {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), connection.ip, connection.port))
        >>>
        >>> my_connection.on_send_raw(callable=con_on_send_raw)
        """
    def on_state_change(self, callable: collections.abc.Callable[[Connection, ConnectionState], None]) -> None:
        """
        set python callback that will be executed on connection state changes

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Connection, c104.ConnectionState], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        connection: c104.Connection
            connection instance
        state: c104.ConnectionState
            latest connection state

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def con_on_state_change(connection: c104.Connection, state: c104.ConnectionState) -> None:
        >>>     print("CON {0}:{1} STATE changed to {2}".format(connection.ip, connection.port, state))
        >>>
        >>> my_connection.on_state_change(callable=con_on_state_change)
        """
    def on_unexpected_message(self, callable: collections.abc.Callable[[Connection, IncomingMessage, Umc], None]) -> None:
        """
        set python callback that will be executed on unexpected incoming messages

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Connection, c104.IncomingMessage, c104.Umc], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        connection: c104.Connection
            connection instance
        message: c104.IncomingMessage
            incoming message
        cause: c104.Umc
            unexpected message cause

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def con_on_unexpected_message(connection: c104.Connection, message: c104.IncomingMessage, cause: c104.Umc) -> None:
        >>>     print("->?| {1} from STATION CA {0}".format(message.common_address, cause))
        >>>
        >>> my_connection.on_unexpected_message(callable=con_on_unexpected_message)
        """
    def remove_station(self, common_address: int) -> bool:
        """
        removes an existing station from this connection

        Parameters
        ----------
        common_address: int
            station common address (value between 1 and 65534)

        Returns
        -------
        bool
            True if the station was successfully removed, otherwise False.

        Example
        -------
        >>> my_connection.remove_station(common_address=12)
        """
    def test(self, common_address: int, with_time: bool = True, wait_for_response: bool = True) -> bool:
        """
        send a test command to the remote terminal unit (server)
        the clients OS time is used

        Parameters
        ----------
        common_address: int
            station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
        with_time: bool
            send with or without timestamp
        wait_for_response: bool
            block call until command success or failure response received?

        Returns
        -------
        bool
            True, if connection is Open, False otherwise

        Example
        -------
        >>> if not my_connection.test(common_address=47):
        >>>     raise ValueError("Cannot send test command")
        """
    def unmute(self) -> bool:
        """
        tell the remote terminal unit (server) that this connection is not muted, allow monitoring messages

        Returns
        -------
        bool
            True, if connection is Open, False otherwise

        Example
        -------
        >>> if not my_connection.unmute():
        >>>     raise ValueError("Cannot unmute connection")
        """
    @property
    def connected_at(self) -> datetime.datetime | None:
        """
        datetime of last connection opening, if connection is open
        """
    @property
    def disconnected_at(self) -> datetime.datetime | None:
        """
        datetime of last connection closing, if connection is closed
        """
    @property
    def has_stations(self) -> bool:
        """
        test if remote server has at least one station
        """
    @property
    def ip(self) -> str:
        """
        remote terminal units (server) ip
        """
    @property
    def is_connected(self) -> bool:
        """
        test if connection is opened
        """
    @property
    def is_muted(self) -> bool:
        """
        test if connection is muted
        """
    @property
    def originator_address(self) -> int:
        """
        originator address of this connection (0-255)
        """
    @originator_address.setter
    def originator_address(self, value: int) -> None:
        """
        assign an originator address for this connection

        Parameters
        ----------
        value: int
            new originator address (0-255)

        Returns
        -------
        None

        Raises
        ------
        ValueError
            not a valid originator address
        """
    @property
    def port(self) -> int:
        """
        remote terminal units (server) port
        """
    @property
    def protocol_parameters(self) -> ProtocolParameters:
        """
        read and update protocol parameters
        """
    @property
    def state(self) -> ConnectionState:
        """
        current connection state
        """
    @property
    def stations(self) -> list[Station]:
        """
        list of all Station objects
        """
class ConnectionState:
    """
    This enum contains all link states for connection state machine behaviour.
    """
    CLOSED: typing.ClassVar[ConnectionState]
    CLOSED_AWAIT_OPEN: typing.ClassVar[ConnectionState]
    CLOSED_AWAIT_RECONNECT: typing.ClassVar[ConnectionState]
    OPEN: typing.ClassVar[ConnectionState]
    OPEN_AWAIT_CLOSED: typing.ClassVar[ConnectionState]
    OPEN_MUTED: typing.ClassVar[ConnectionState]
    __members__: typing.ClassVar[dict[str, ConnectionState]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Cot:
    """
    This enum contains all valid IEC60870 transmission cause identifier to interpret message context.
    """
    ACTIVATION: typing.ClassVar[Cot]
    ACTIVATION_CON: typing.ClassVar[Cot]
    ACTIVATION_TERMINATION: typing.ClassVar[Cot]
    AUTHENTICATION: typing.ClassVar[Cot]
    BACKGROUND_SCAN: typing.ClassVar[Cot]
    DEACTIVATION: typing.ClassVar[Cot]
    DEACTIVATION_CON: typing.ClassVar[Cot]
    FILE_TRANSFER: typing.ClassVar[Cot]
    INITIALIZED: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_1: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_10: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_11: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_12: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_13: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_14: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_15: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_16: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_2: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_3: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_4: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_5: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_6: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_7: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_8: typing.ClassVar[Cot]
    INTERROGATED_BY_GROUP_9: typing.ClassVar[Cot]
    INTERROGATED_BY_STATION: typing.ClassVar[Cot]
    MAINTENANCE_OF_AUTH_SESSION_KEY: typing.ClassVar[Cot]
    MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY: typing.ClassVar[Cot]
    PERIODIC: typing.ClassVar[Cot]
    REQUEST: typing.ClassVar[Cot]
    REQUESTED_BY_GENERAL_COUNTER: typing.ClassVar[Cot]
    REQUESTED_BY_GROUP_1_COUNTER: typing.ClassVar[Cot]
    REQUESTED_BY_GROUP_2_COUNTER: typing.ClassVar[Cot]
    REQUESTED_BY_GROUP_3_COUNTER: typing.ClassVar[Cot]
    REQUESTED_BY_GROUP_4_COUNTER: typing.ClassVar[Cot]
    RETURN_INFO_LOCAL: typing.ClassVar[Cot]
    RETURN_INFO_REMOTE: typing.ClassVar[Cot]
    SPONTANEOUS: typing.ClassVar[Cot]
    UNKNOWN_CA: typing.ClassVar[Cot]
    UNKNOWN_COT: typing.ClassVar[Cot]
    UNKNOWN_IOA: typing.ClassVar[Cot]
    UNKNOWN_TYPE_ID: typing.ClassVar[Cot]
    __members__: typing.ClassVar[dict[str, Cot]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Debug:
    """
    This enum contains all valid debug bits to interpret and manipulate debug mode.
    """
    All: typing.ClassVar[Debug]
    Callback: typing.ClassVar[Debug]
    Client: typing.ClassVar[Debug]
    Connection: typing.ClassVar[Debug]
    Gil: typing.ClassVar[Debug]
    Message: typing.ClassVar[Debug]
    Point: typing.ClassVar[Debug]
    Server: typing.ClassVar[Debug]
    Station: typing.ClassVar[Debug]
    __members__: typing.ClassVar[dict[str, Debug]]
    def is_any(self) -> bool:
        """
        test if there are any bits set
        """
    def is_none(self) -> bool:
        """
        test if no bits are set
        """
    @property
    def value(self) -> int:
        """
        combined bits in integer representation
        """
class Double:
    """
    This enum contains all valid IEC60870 step command values to identify and send step commands.
    """
    INDETERMINATE: typing.ClassVar[Double]
    INTERMEDIATE: typing.ClassVar[Double]
    OFF: typing.ClassVar[Double]
    ON: typing.ClassVar[Double]
    __members__: typing.ClassVar[dict[str, Double]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> bool:
        ...
class DoubleCmd(Information):
    """
    This class represents all specific double command information
    """
    def __init__(self, state: Double, qualifier: Qoc = Qoc.NONE, recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new double command

        Parameters
        ----------
        state: c104.Double
            Double command value
        qualifier: c104.Qoc
            Qualifier of command
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> double_cmd = c104.DoubleCmd(state=c104.Double.ON, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def qualifier(self) -> Qoc:
        """
        the command qualifier information
        """
    @property
    def quality(self) -> None:
        """
        This information does not contain quality information.
        """
    @property
    def state(self) -> Double:
        """
        the value
        """
    @property
    def value(self) -> Double:
        """
        references property ``state``

        The setter is available via point.value=xyz
        """
class DoubleInfo(Information):
    """
    This class represents all specific double point information
    """
    def __init__(self, state: Double, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new double info

        Parameters
        ----------
        state: c104.Double
            Double point status value
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> double_info = c104.DoubleInfo(state=c104.Double.ON, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def state(self) -> Double:
        """
        the value
        """
    @property
    def value(self) -> Double:
        """
        references property ``state``

        The setter is available via point.value=xyz
        """
class EventState:
    """
    This enum contains all valid IEC60870 event states to interpret and send event messages.
    """
    INDETERMINATE: typing.ClassVar[EventState]
    INTERMEDIATE: typing.ClassVar[EventState]
    OFF: typing.ClassVar[EventState]
    ON: typing.ClassVar[EventState]
    __members__: typing.ClassVar[dict[str, EventState]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Frz:
    """
    This enum contains all valid IEC60870 freeze behaviour for a counter interrogation command.
    """
    READ: typing.ClassVar[Frz]
    FREEZE_WITHOUT_RESET: typing.ClassVar[Frz]
    FREEZE_WITH_RESET: typing.ClassVar[Frz]
    COUNTER_RESET: typing.ClassVar[Frz]
    __members__: typing.ClassVar[dict[str, Frz]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class IncomingMessage:
    """
    This class represents incoming messages and provides access to structured properties interpreted from incoming messages
    """
    def first(self) -> None:
        """
        reset message information element pointer to first position

        Returns
        -------
        None
        """
    def next(self) -> bool:
        """
        move message information element pointer to next position, starting by first one

        Returns
        -------
        bool
            True, if another information element exists, otherwise False
        """
    @property
    def common_address(self) -> int:
        """
        common address (1-65534)
        """
    @property
    def cot(self) -> Cot:
        """
        cause of transmission
        """
    @property
    def info(self) -> Information:
        """
        value
        """
    @property
    def io_address(self) -> int:
        """
        information object address (0-16777215)
        """
    @property
    def is_negative(self) -> bool:
        """
        test if negative flag is set
        """
    @property
    def is_select_command(self) -> bool:
        """
        test if message is a point command and has select flag set
        """
    @property
    def is_sequence(self) -> bool:
        """
        test if sequence flag is set
        """
    @property
    def is_test(self) -> bool:
        """
        test if test flag is set
        """
    @property
    def number_of_object(self) -> int:
        """
        represents the number of information objects

        Deprecated: This property is deprecated and will be removed in version 3.0.0.
        Use ``number_of_objects`` instead.
        """
    @property
    def number_of_objects(self) -> int:
        """
        represents the number of information objects contained in this message
        """
    @property
    def originator_address(self) -> int:
        """
        originator address (0-255)
        """
    @property
    def raw(self) -> bytes:
        """
        raw ASDU message bytes
        """
    @property
    def raw_explain(self) -> str:
        """
        ASDU message bytes explained
        """
    @property
    def type(self) -> Type:
        """
        IEC60870 message type identifier
        """
class Information:
    """
    This class represents all specialized kind of information a specific point may have
    """
    @property
    def processed_at(self) -> datetime.datetime:
        """
        timestamp with milliseconds of last local information processing
        """
    @property
    def quality(self) -> typing.Union[None, Quality, BinaryCounterQuality]:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def recorded_at(self) -> datetime.datetime | None:
        """
        timestamp with milliseconds transported with the value itself or None
        """
    @property
    def value(self) -> None | bool | Double | Step | Int7 | Int16 | int | Byte32 | NormalizedFloat | float | EventState | StartEvents | OutputCircuits | PackedSingle:
        """
        the mapped primary information value property

        The setter is available via point.value=xyz
        """
class Init:
    """
    This enum contains all connection init command options. Everytime the connection is established the client will behave as follows:
    """
    ALL: typing.ClassVar[Init]
    CLOCK_SYNC: typing.ClassVar[Init]
    INTERROGATION: typing.ClassVar[Init]
    MUTED: typing.ClassVar[Init]
    NONE: typing.ClassVar[Init]
    __members__: typing.ClassVar[dict[str, Init]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Int16:
    def __init__(self, value: int) -> None:
        """
        create 16 bit fixed-length signed integer

        Parameters
        ----------
        value: int
            the value

        Raises
        ------
        ValueError
            cannot convert value to 16 bit signed integer
        """
    @property
    def max(self) -> int:
        """
        maximum value
        """
    @property
    def min(self) -> int:
        """
        minimum value
        """
class Int7:
    def __init__(self, value: int) -> None:
        """
        create 7 bit fixed-length signed integer

        Parameters
        ----------
        value: int
            the value

        Raises
        ------
        ValueError
            cannot convert value to 7 bit signed integer
        """
    @property
    def max(self) -> int:
        """
        maximum value
        """
    @property
    def min(self) -> int:
        """
        minimum value
        """
class NormalizedCmd(Information):
    """
    This class represents all specific normalized set point command information
    """
    def __init__(self, target: NormalizedFloat, qualifier: UInt7 = UInt7(0), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new normalized set point command

        Parameters
        ----------
        target: c104.NormalizedFloat
            Target set-point value [-1.f, 1.f]
        qualifier: c104.UInt7
            Qualifier of set-point command
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> normalized_cmd = c104.NormalizedCmd(target=c104.NormalizedFloat(23.45), qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def qualifier(self) -> UInt7:
        """
        the command qualifier information
        """
    @property
    def quality(self) -> None:
        """
        This information does not contain quality information.
        """
    @property
    def target(self) -> NormalizedFloat:
        """
        the value
        """
    @property
    def value(self) -> NormalizedFloat:
        """
        references property ``target``

        The setter is available via point.value=xyz
        """
class NormalizedFloat:
    def __init__(self, value: int | float) -> None:
        """
        create normalized float between -1 and 1

        Parameters
        ----------
        value: int | float
            the value

        Raises
        ------
        ValueError
            cannot convert value to normalized float
        """
    @property
    def max(self) -> float:
        """
        maximum value
        """
    @property
    def min(self) -> float:
        """
        minimum value
        """
class NormalizedInfo(Information):
    """
    This class represents all specific normalized measurement point information
    """
    def __init__(self, actual: NormalizedFloat, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new normalized measurement info

        Parameters
        ----------
        actual: c104.NormalizedFloat
            Actual measurement value [-1.f, 1.f]
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> normalized_info = c104.NormalizedInfo(actual=c104.NormalizedFloat(23.45), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def actual(self) -> NormalizedFloat:
        """
        the value
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def value(self) -> NormalizedFloat:
        """
        references property ``actual``

        The setter is available via point.value=xyz
        """
class OutputCircuits:
    """
    This enum contains all Output Circuit bits to interpret and manipulate protection equipment messages.
    """
    General: typing.ClassVar[OutputCircuits]
    PhaseL1: typing.ClassVar[OutputCircuits]
    PhaseL2: typing.ClassVar[OutputCircuits]
    PhaseL3: typing.ClassVar[OutputCircuits]
    __members__: typing.ClassVar[dict[str, OutputCircuits]]
    def is_any(self) -> bool:
        """
        test if there are any bits set
        """
    def is_none(self) -> bool:
        """
        test if no bits are set
        """
    @property
    def value(self) -> str:
        """
        combined bits in integer representation
        """
class PackedSingle:
    """
    This enum contains all State bits to interpret and manipulate status with change detection messages.
    """
    I0: typing.ClassVar[PackedSingle]
    I1: typing.ClassVar[PackedSingle]
    I10: typing.ClassVar[PackedSingle]
    I11: typing.ClassVar[PackedSingle]
    I12: typing.ClassVar[PackedSingle]
    I13: typing.ClassVar[PackedSingle]
    I14: typing.ClassVar[PackedSingle]
    I15: typing.ClassVar[PackedSingle]
    I2: typing.ClassVar[PackedSingle]
    I3: typing.ClassVar[PackedSingle]
    I4: typing.ClassVar[PackedSingle]
    I5: typing.ClassVar[PackedSingle]
    I6: typing.ClassVar[PackedSingle]
    I7: typing.ClassVar[PackedSingle]
    I8: typing.ClassVar[PackedSingle]
    I9: typing.ClassVar[PackedSingle]
    __members__: typing.ClassVar[dict[str, PackedSingle]]
    def is_any(self) -> bool:
        """
        test if there are any bits set
        """
    def is_none(self) -> bool:
        """
        test if no bits are set
        """
    @property
    def value(self) -> int:
        """
        combined bits in integer representation
        """
class Point:
    """
    This class represents command and measurement data point of a station and provides access to structured properties of points
    """
    def on_before_auto_transmit(self, callable: collections.abc.Callable[[Point], None]) -> None:
        """
        set python callback that will be called before server reports a measured value interval-based

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Point], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly, parent station reference is invalid or function is called from client context

        **Callable signature**

        Callable Parameters
        -------------------
        point: c104.Point
            point instance

        Callable Returns
        ----------------
        None

        Warning
        -------
        The difference between **on_before_read** and **on_before_auto_transmit** is the calling context.
        **on_before_read** is called when a client sends a command to report a point (interrogation or read).
        **on_before_auto_transmit** is called when the server reports a measured value interval-based.

        Example
        -------
        >>> def on_before_auto_transmit_step(point: c104.Point) -> None:
        >>>     print("SV] {0} PERIODIC TRANSMIT on IOA: {1}".format(point.type, point.io_address))
        >>>     point.value = c104.Int7(random.randint(-64,63))  # import random
        >>>
        >>> step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1, report_ms=2000)
        >>> step_point.on_before_auto_transmit(callable=on_before_auto_transmit_step)
        """
    def on_before_read(self, callable: collections.abc.Callable[[Point], None]) -> None:
        """
        set python callback that will be called on incoming interrogation or read commands to support polling

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Point], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly, parent station reference is invalid or function is called from client context

        **Callable signature**

        Callable Parameters
        -------------------
        point: c104.Point
            point instance

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def on_before_read_steppoint(point: c104.Point) -> None:
        >>>     print("SV] {0} READ COMMAND on IOA: {1}".format(point.type, point.io_address))
        >>>     point.value = random.randint(-64,63)  # import random
        >>>
        >>> step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1, report_ms=2000)
        >>> step_point.on_before_read(callable=on_before_read_steppoint)
        """
    def on_receive(self, callable: collections.abc.Callable[[Point, Information, IncomingMessage], ResponseState]) -> None:
        """
        set python callback that will be executed on every incoming message
        this can be either a command or an monitoring message

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Point, c104.Information, c104.IncomingMessage], c104.ResponseState]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        point: c104.Point
            point instance
        previous_info: c104.Information
            Information object containing the state of the point before the command took effect
        message: c104.IncomingMessage
            new command message

        Callable Returns
        ----------------
        c104.ResponseState
            send command SUCCESS or FAILURE response

        Example
        -------
        >>> def on_setpoint_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
        >>>     print("SV] {0} SETPOINT COMMAND on IOA: {1}, new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_info, message.cot, point.quality))
        >>>     if point.related_io_address:
        >>>         print("SV] -> RELATED IO ADDRESS: {}".format(point.related_io_address))
        >>>         related_point = sv_station_2.get_point(point.related_io_address)
        >>>         if related_point:
        >>>             print("SV] -> RELATED POINT VALUE UPDATE")
        >>>             related_point.value = point.value
        >>>         else:
        >>>             print("SV] -> RELATED POINT NOT FOUND!")
        >>>     return c104.ResponseState.SUCCESS
        >>>
        >>> sv_measurement_point = sv_station_2.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
        >>> sv_measurement_point.value = 12.34
        >>> sv_command_point = sv_station_2.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=sv_measurement_point.io_address, related_io_autoreturn=True, command_mode=c104.CommandMode.SELECT_AND_EXECUTE)
        >>> sv_command_point.on_receive(callable=on_setpoint_command)
        """
    def on_timer(self, callable: collections.abc.Callable[[Point], None], int) -> None:
        """
        set python callback that will be called in a fixed delay (timer_ms)

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Point], None]
            callback function reference
        interval_ms: int
            interval between two callback executions in milliseconds

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly, parent station reference is invalid or function is called from client context

        **Callable signature**

        Callable Parameters
        -------------------
        point: c104.Point
            point instance
        interval_ms: int
            fixed delay between timer callback execution, default: 0, min: 50

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def on_timer(point: c104.Point) -> None:
        >>>     print("SV] {0} TIMER on IOA: {1}".format(point.type, point.io_address))
        >>>     point.value = random.randint(-64,63)  # import random
        >>>
        >>> nv_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ME_TD_1)
        >>> nv_point.on_timer(callable=on_timer, interval_ms=1000)
        """
    def read(self) -> bool:
        """
        send read command

        Returns
        -------
        bool
            True if the command was successfully accepted by the server, otherwise False

        Raises
        ------
        ValueError
            parent station or connection reference is invalid or called from remote terminal unit (server) context

        Example
        -------
        >>> if cl_step_point.read():
        >>>     print("read command successful")
        """
    def transmit(self, cause: Cot) -> bool:
        """
        **Server-side point**
        report a measurement value to connected clients

        **Client-side point**
        send the command point to the server

        Parameters
        ----------
        cause: c104.Cot
            cause of the transmission

        Raises
        ------
        ValueError
            parent station, server or connection reference is invalid

        Returns
        -------
        bool
            True if the command was successfully send (server-side) or accepted by the server (client-side), otherwise False

        Example
        -------
        >>> sv_measurement_point.transmit(cause=c104.Cot.SPONTANEOUS)
        >>> cl_single_command_point.transmit(cause=c104.Cot.ACTIVATION)
        """
    @property
    def command_mode(self) -> CommandMode:
        """
        command transmission mode (direct or select-and-execute)
        """
    @command_mode.setter
    def command_mode(self, value: CommandMode) -> None:
        """
        set command transmission mode (direct or select-and-execute)

        Parameters
        ----------
        value: c104.CommandMode
            new command transmission mode

        Returns
        -------
        None
        """
    @property
    def info(self) -> Information:
        """
        current information
        """
    @info.setter
    def info(self, value: Information) -> None:
        """
        assign new information

        Parameters
        ----------
        value: c104.Information
            new information

        Returns
        -------
        None

        Raises
        ------
        ValueError
            new information type does not match current information type
        """
    @property
    def io_address(self) -> int:
        """
        information object address
        """
    @property
    def processed_at(self) -> datetime.datetime:
        """
        timestamp with milliseconds of last local information processing
        """
    @property
    def quality(self) -> None | Quality | BinaryCounterQuality:
        """
        the quality

        this is just a shortcut to point.info.quality
        """
    @quality.setter
    def quality(self, value: None | Quality | BinaryCounterQuality) -> None:
        """
        assign new quality

        Parameters
        ----------
        value: typing.Union[None, c104.Quality, c104.BinaryCounterQuality]
            new quality

        Returns
        -------
        None

        Raises
        ------
        ValueError
            new quality type does not match current quality type
        """
    @property
    def recorded_at(self) -> datetime.datetime | None:
        """
        timestamp with milliseconds transported with the value itself or None
        """
    @property
    def related_io_address(self) -> int | None:
        """
        io_address of a related monitoring point or None
        """
    @related_io_address.setter
    def related_io_address(self, value: int | None) -> None:
        """
        assign new related point identified by its information object address

        Parameters
        ----------
        value: int | None
            information object address of related point

        Returns
        -------
        None

        Raises
        ------
        ValueError
            invalid information object address
        """
    @property
    def related_io_autoreturn(self) -> bool:
        """
        automatic transmission of return info remote messages for related point on incoming client command (only for control points)
        """
    @related_io_autoreturn.setter
    def related_io_autoreturn(self, value: bool) -> None:
        """
        enabled or disable automatic transmission of return info remote messages for related point on incoming client command (only for control points)

        Parameters
        ----------
        value: bool
            state of automatic related response

        Returns
        -------
        None
        """
    @property
    def report_ms(self) -> int:
        """
        interval in milliseconds between periodic transmission

        0 = no periodic transmission
        """
    @report_ms.setter
    def report_ms(self, value: int) -> None:
        """
        set periodic transmission interval

        Parameters
        ----------
        value: int
            interval in milliseconds between periodic transmission, 0 = no periodic transmission

        Returns
        -------
        None

        Raises
        ------
        ValueError
            not a positive integer multiple of tick_rate_ms
        ValueError
            not a monitoring point
        ValueError
            not a local point
        """
    @property
    def selected_by(self) -> int | None:
        """
        originator address (0-255) or None
        """
    @property
    def station(self) -> Station | None:
        """
        parent Station object
        """
    @property
    def timer_ms(self) -> int:
        """
        interval in milliseconds between timer callbacks, 0 = no periodic transmission
        """
    @property
    def type(self) -> Type:
        """
        data related IEC60870 message type identifier
        """
    @property
    def value(self) -> None | bool | Double | Step | Int7 | Int16 | int | Byte32 | NormalizedFloat | float | EventState | StartEvents | OutputCircuits | PackedSingle:
        """
        the primary information value

        this is just a shortcut to point.info.value
        """
    @value.setter
    def value(self, value: None | bool | Double | Step | Int7 | Int16 | int | Byte32 | NormalizedFloat | float | EventState | StartEvents | OutputCircuits | PackedSingle) -> None:
        """
        update the primary information value

        Parameters
        ----------
        value: typing.Union[None, bool, c104.Double, c104.Step, c104.Int7, c104.Int16, int, c104.Byte32, c104.NormalizedFloat, float, c104.EventState, c104.StartEvents, c104.OutputCircuits, c104.PackedSingle]
            new primary information value, please use the value type fitting to your point

        Returns
        -------
        None

        Raises
        ------
        ValueError
            new value type does not match current value type
        """
class ProtectionCircuitInfo(Information):
    """
    This class represents all specific protection equipment output circuit point information
    """
    def __init__(self, circuits: OutputCircuits, relay_operating_ms: UInt16, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new output circuits info raised by protection equipment

        Parameters
        ----------
        circuits: c104.OutputCircuits
            Set of output circuits
        relay_operating_ms: c104.UInt16
            Time in milliseconds of relay operation
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> output_circuits = c104.ProtectionCircuitInfo(events=c104.OutputCircuits.PhaseL1|c104.OutputCircuits.PhaseL2, relay_operating_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def circuits(self) -> OutputCircuits:
        """
        the started events
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def relay_operating_ms(self) -> int:
        """
        the relay operation duration information
        """
    @property
    def value(self) -> OutputCircuits:
        """
        references property ``circuits``

        The setter is available via point.value=xyz
        """
class ProtectionEventInfo(Information):
    """
    This class represents all specific protection equipment single event point information
    """
    def __init__(self, state: EventState, elapsed_ms: UInt16, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new event info raised by protection equipment

        Parameters
        ----------
        state: c104.EventState
            State of the event
        elapsed_ms: c104.UInt16
            Time in milliseconds elapsed
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> single_event = c104.ProtectionEventInfo(state=c104.EventState.ON, elapsed_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def elapsed_ms(self) -> int:
        """
        the elapsed time in milliseconds
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def state(self) -> EventState:
        """
        the state
        """
    @property
    def value(self) -> EventState:
        """
        references property ``state``

        The setter is available via point.value=xyz
        """
class ProtectionStartInfo(Information):
    """
    This class represents all specific protection equipment packed start events point information
    """
    def __init__(self, events: StartEvents, relay_duration_ms: UInt16, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new packed event start info raised by protection equipment

        Parameters
        ----------
        events: c104.StartEvents
            Set of start events
        relay_duration_ms: c104.UInt16
            Time in milliseconds of relay duration
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> start_events = c104.ProtectionStartInfo(events=c104.StartEvents.ON, relay_duration_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def events(self) -> StartEvents:
        """
        the started events
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def relay_duration_ms(self) -> int:
        """
        the relay duration information
        """
    @property
    def value(self) -> StartEvents:
        """
        references property ``events``

        The setter is available via point.value=xyz
        """
class ProtocolParameters:
    """
    This class is used to configure protocol parameters for server and client
    """
    @property
    def confirm_interval(self) -> int:
        """
        maximum interval to acknowledge received messages (ms) (property name: t2)
        """
    @confirm_interval.setter
    def confirm_interval(self, value: int) -> None:
        """
        set send acknowledge timeout (ms) (property name: t2)

        Parameters
        ----------
        value: int
            new timeout

        Returns
        -------
        None
        """
    @property
    def connection_timeout(self) -> int:
        """
        socket connection timeout (ms) (property name: t0)
        """
    @connection_timeout.setter
    def connection_timeout(self, value: int) -> None:
        """
        set socket connection timeout (ms) (property name: t0)

        Parameters
        ----------
        value: int
            new timeout

        Returns
        -------
        None
        """
    @property
    def keep_alive_interval(self) -> int:
        """
        maximum interval without communication, send test frame message to prove liveness (ms) (property name: t3)
        """
    @keep_alive_interval.setter
    def keep_alive_interval(self, value: int) -> None:
        """
        set timeout to send test frame message to prove liveness (ms), if connection silent (property name: t3)

        Parameters
        ----------
        value: int
            new timeout

        Returns
        -------
        None
        """
    @property
    def message_timeout(self) -> int:
        """
        timeout for sent messages to be acknowledged by counterparty (ms) (property name: t1)
        """
    @message_timeout.setter
    def message_timeout(self, value: int) -> None:
        """
        set timeout for sent messages to be acknowledged by counterparty (ms) (property name: t1)

        Parameters
        ----------
        value: int
            new timeout

        Returns
        -------
        None
        """
    @property
    def receive_window_size(self) -> int:
        """
        threshold of unconfirmed incoming messages to send acknowledgments (property name: w)
        """
    @receive_window_size.setter
    def receive_window_size(self, value: int) -> None:
        """
        set threshold of unconfirmed incoming messages to send acknowledgments (property name: w)

        Parameters
        ----------
        value: int
            new threshold

        Returns
        -------
        None
        """
    @property
    def send_window_size(self) -> int:
        """
        threshold of unconfirmed outgoing messages, before waiting for acknowledgments (property name: k)
        """
    @send_window_size.setter
    def send_window_size(self, value: int) -> None:
        """
        set threshold of unconfirmed outgoing messages, before waiting for acknowledgments (property name: k)

        Parameters
        ----------
        value: int
            new threshold

        Returns
        -------
        None
        """
class Qoc:
    """
    This enum contains all valid IEC60870 qualifier of command duration options.
    """
    LONG_PULSE: typing.ClassVar[Qoc]
    NONE: typing.ClassVar[Qoc]
    PERSISTENT: typing.ClassVar[Qoc]
    SHORT_PULSE: typing.ClassVar[Qoc]
    __members__: typing.ClassVar[dict[str, Qoc]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Qoi:
    """
    This enum contains all valid IEC60870 qualifier for an interrogation command.
    """
    GROUP_1: typing.ClassVar[Qoi]
    GROUP_10: typing.ClassVar[Qoi]
    GROUP_11: typing.ClassVar[Qoi]
    GROUP_12: typing.ClassVar[Qoi]
    GROUP_13: typing.ClassVar[Qoi]
    GROUP_14: typing.ClassVar[Qoi]
    GROUP_15: typing.ClassVar[Qoi]
    GROUP_16: typing.ClassVar[Qoi]
    GROUP_2: typing.ClassVar[Qoi]
    GROUP_3: typing.ClassVar[Qoi]
    GROUP_4: typing.ClassVar[Qoi]
    GROUP_5: typing.ClassVar[Qoi]
    GROUP_6: typing.ClassVar[Qoi]
    GROUP_7: typing.ClassVar[Qoi]
    GROUP_8: typing.ClassVar[Qoi]
    GROUP_9: typing.ClassVar[Qoi]
    STATION: typing.ClassVar[Qoi]
    __members__: typing.ClassVar[dict[str, Qoi]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Quality:
    """
    This enum contains all quality issue bits to interpret and manipulate measurement quality.
    """
    Blocked: typing.ClassVar[Quality]
    ElapsedTimeInvalid: typing.ClassVar[Quality]
    Invalid: typing.ClassVar[Quality]
    NonTopical: typing.ClassVar[Quality]
    Overflow: typing.ClassVar[Quality]
    Substituted: typing.ClassVar[Quality]
    __members__: typing.ClassVar[dict[str, Quality]]
    def is_any(self) -> bool:
        """
        test if there are any flags set
        """
    def is_good(self) -> bool:
        """
        test if no quality problems are set
        """
    @property
    def value(self) -> int:
        """
        combined bits in integer representation
        """
class ResponseState:
    """
    This enum contains all command response states, that add the ability to control the servers command response behaviour via python callbacks return value.
    """
    FAILURE: typing.ClassVar[ResponseState]
    NONE: typing.ClassVar[ResponseState]
    SUCCESS: typing.ClassVar[ResponseState]
    __members__: typing.ClassVar[dict[str, ResponseState]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Rqt:
    """
    This enum contains all valid IEC60870 qualifier for a counter interrogation command.
    """
    GENERAL: typing.ClassVar[Rqt]
    GROUP_1: typing.ClassVar[Rqt]
    GROUP_2: typing.ClassVar[Rqt]
    GROUP_3: typing.ClassVar[Rqt]
    GROUP_4: typing.ClassVar[Rqt]
    __members__: typing.ClassVar[dict[str, Rqt]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class ScaledCmd(Information):
    """
    This class represents all specific scaled set point command information
    """
    def __init__(self, target: Int16, qualifier: UInt7 = UInt7(0), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new scaled set point command

        Parameters
        ----------
        target: c104.Int16
            Target set-point value [-32768, 32767]
        qualifier: c104.UInt7
            Qualifier of set-point command
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> scaled_cmd = c104.ScaledCmd(target=c104.Int16(-2345), qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def qualifier(self) -> UInt7:
        """
        the command qualifier information
        """
    @property
    def quality(self) -> None:
        """
        This information does not contain quality information.
        """
    @property
    def target(self) -> Int16:
        """
        the value
        """
    @property
    def value(self) -> Int16:
        """
        references property ``target``

        The setter is available via point.value=xyz
        """
class ScaledInfo(Information):
    """
    This class represents all specific scaled measurement point information
    """
    def __init__(self, actual: Int16, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new scaled measurement info

        Parameters
        ----------
        actual: c104.Int16
            Actual measurement value [-32768, 32767]
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> scaled_info = c104.ScaledInfo(actual=c104.Int16(-2345), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def actual(self) -> Int16:
        """
        the value
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def value(self) -> Int16:
        """
        references property ``actual``

        The setter is available via point.value=xyz
        """
class Server:
    """
    This class represents a local server and provides access to meta information and containing stations
    """
    def __init__(self, ip: str = "0.0.0.0", port: int = 2404, tick_rate_ms: int = 100, select_timeout_ms = 100, max_connections: int = 0, transport_security: TransportSecurity | None = None) -> None:
        """
        create a new 104er server

        Parameters
        ----------
        ip: str
            listening server ip address
        port:int
            listening server port
        tick_rate_ms: int
            server thread update interval
        select_timeout_ms: int
            execution for points in SELECT_AND_EXECUTE mode must arrive within this interval to succeed
        max_connections: int
            maximum number of clients allowed to connect
        transport_security: c104.TransportSecurity, optional
            TLS configuration object

        Example
        -------
        >>> my_server = c104.Server(ip="0.0.0.0", port=2404, tick_rate_ms=100, select_timeout_ms=100, max_connections=0)
        """
    def add_station(self, common_address: int) -> Station | None:
        """
        add a new station to this server and return the new station object

        Parameters
        ----------
        common_address: int
            station common address (value between 1 and 65534)

        Returns
        -------
        c104.Station, optional
            station object, if station was added, else None

        Example
        -------
        >>> station_1 = my_server.add_station(common_address=15)
        """
    def get_station(self, common_address: int) -> Station | None:
        """
        get a station object via common address

        Parameters
        ----------
        common_address: int
            station common address (value between 1 and 65534)

        Returns
        -------
        c104.Station, optional
            station object, if found, else None

        Example
        -------
        >>> station_2 = my_server.get_connection(common_address=14)
        """
    def on_clock_sync(self, callable: collections.abc.Callable[[Server, str, datetime.datetime], ResponseState]) -> None:
        """
        set python callback that will be executed on incoming clock sync command

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Server, str, datetime.datetime], c104.ResponseState]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        server: c104.Server
            server instance
        ip: str
            client connection request ip
        date_time: datetime.datetime
            clients current clock time

        Callable Returns
        ----------------
        c104.ResponseState
            success or failure of clock sync command

        Example
        -------
        >>> import datetime
        >>>
        >>> def sv_on_clock_sync(server: c104.Server, ip: str, date_time: datetime.datetime) -> c104.ResponseState:
        >>>     print("->@| Time {0} from {1} | SERVER {2}:{3}".format(date_time, ip, server.ip, server.port))
        >>>     return c104.ResponseState.SUCCESS
        >>>
        >>> my_server.on_clock_sync(callable=sv_on_clock_sync)
        """
    def on_connect(self, callable: collections.abc.Callable[[Server, ip], bool]) -> None:
        """
        set python callback that will be executed on incoming connection requests

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Server, ip], bool]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        server: c104.Server
            server instance
        ip: str
            client connection request ip

        Callable Returns
        ----------------
        bool
            accept or reject the connection request

        Example
        -------
        >>> def sv_on_connect(server: c104.Server, ip: str) -> bool:
        >>>     print("<->| {0} | SERVER {1}:{2}".format(ip, server.ip, server.port))
        >>>     return ip == "127.0.0.1"
        >>>
        >>> my_server.on_connect(callable=sv_on_connect)
        """
    def on_receive_raw(self, callable: collections.abc.Callable[[Server, bytes], None]) -> None:
        """
        set python callback that will be executed on incoming message

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Server, bytes], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        server: c104.Server
            server instance
        data: bytes
            raw message bytes

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
        >>>     print("-->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
        >>>
        >>> my_server.on_receive_raw(callable=sv_on_receive_raw)
        """
    def on_send_raw(self, callable: collections.abc.Callable[[Server, bytes], None]) -> None:
        """
        set python callback that will be executed on outgoing message

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Server, bytes], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        server: c104.Server
            server instance
        data: bytes
            raw message bytes

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def sv_on_send_raw(server: c104.Server, data: bytes) -> None:
        >>>     print("<--| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
        >>>
        >>> my_server.on_send_raw(callable=sv_on_send_raw)
        """
    def on_unexpected_message(self, callable: collections.abc.Callable[[Server, IncomingMessage, Umc], None]) -> None:
        """
        set python callback that will be executed on unexpected incoming messages

        Parameters
        ----------
        callable: collections.abc.Callable[[c104.Server, c104.IncomingMessage, c104.Umc], None]
            callback function reference

        Returns
        -------
        None

        Raises
        ------
        ValueError
            callable signature does not match exactly

        **Callable signature**

        Callable Parameters
        -------------------
        server: c104.Server
            server instance
        message: c104.IncomingMessage
            incoming message
        cause: c104.Umc
            unexpected message cause

        Callable Returns
        ----------------
        None

        Example
        -------
        >>> def sv_on_unexpected_message(server: c104.Server, message: c104.IncomingMessage, cause: c104.Umc) -> None:
        >>>     print("->?| {1} from CLIENT OA {0} | SERVER {2}:{3}".format(message.originator_address, cause, server.ip, server.port))
        >>>
        >>> my_server.on_unexpected_message(callable=sv_on_unexpected_message)
        """
    def remove_station(self, common_address: int) -> bool:
        """
        removes an existing station from this server

        Parameters
        ----------
        common_address: int
            station common address (value between 1 and 65534)

        Returns
        -------
        bool
            True if the station was successfully removed, otherwise False.

        Example
        -------
        >>> station_3.remove_station(common_address=12)
        """
    def start(self) -> None:
        """
        open local server socket for incoming connections

        Raises
        ------
        RuntimeError
            server thread failed to start

        Example
        -------
        >>> my_server.start()
        """
    def stop(self) -> None:
        """
        stop local server socket

        Example
        -------
        >>> my_server.stop()
        """
    def transmit_batch(self, batch: Batch) -> bool:
        """
        transmit a batch object

        Parameters
        ----------
        batch: c104.Batch
            batch object to transmit

        Returns
        -------
        bool
            send success

        Example
        -------
        >>> success = my_server.transmit_batch(c104.Batch([point1, point2, point3]))
        """
    @property
    def active_connection_count(self) -> int:
        """
        get number of active (open and not muted) connections to clients
        """
    @property
    def has_active_connections(self) -> bool:
        """
        test if server has active (open and not muted) connections to clients
        """
    @property
    def has_open_connections(self) -> bool:
        """
        test if server has open connections to clients
        """
    @property
    def has_stations(self) -> bool:
        """
        test if server has at least one station
        """
    @property
    def ip(self) -> str:
        """
        ip address the server will accept connections on, "0.0.0.0" = any
        """
    @property
    def is_running(self) -> bool:
        """
        test if server is running
        """
    @property
    def max_connections(self) -> int:
        """
        maximum number of open connections, 0 = no limit
        """
    @max_connections.setter
    def max_connections(self, value: int) -> None:
        """
        set maximum number of open connections

        Parameters
        ----------
        value: int
            maximum number of open connections, 0 = no limit

        Returns
        -------
        None

        Raises
        ------
        ValueError
            not a positive integer
        """
    @property
    def open_connection_count(self) -> int:
        """
        represents the number of open connections to clients
        """
    @property
    def port(self) -> int:
        """
        port number the server will accept connections on
        """
    @property
    def protocol_parameters(self) -> ProtocolParameters:
        """
        read and update protocol parameters
        """
    @property
    def stations(self) -> list[Station]:
        """
        list of all local Station objects
        """
    @property
    def tick_rate_ms(self) -> int:
        """
        the servers tick rate in milliseconds
        """
class ShortCmd(Information):
    """
    This class represents all specific short set point command information
    """
    def __init__(self, target: float, qualifier: UInt7 = UInt7(0), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new short set point command

        Parameters
        ----------
        target: float
            Target set-point value in 32-bit precision
        qualifier: c104.UInt7
            Qualifier of set-point command
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> short_cmd = c104.ShortCmd(target=-23.45, qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def qualifier(self) -> UInt7:
        """
        the command qualifier information
        """
    @property
    def quality(self) -> None:
        """
        This information does not contain quality information.
        """
    @property
    def target(self) -> float:
        """
        the value
        """
    @property
    def value(self) -> float:
        """
        references property ``target``

        The setter is available via point.value=xyz
        """
class ShortInfo(Information):
    """
    This class represents all specific short measurement point information
    """
    def __init__(self, actual: float, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new short measurement info

        Parameters
        ----------
        actual: float
            Actual measurement value in 32-bit precision
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> short_info = c104.ShortInfo(actual=23.45, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def actual(self) -> float:
        """
        the value
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def value(self) -> float:
        """
        references property ``actual``

        The setter is available via point.value=xyz
        """
class SingleCmd(Information):
    """
    This class represents all specific single command information
    """
    def __init__(self, on: bool, qualifier: Qoc = Qoc.NONE, recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new single command

        Parameters
        ----------
        on: bool
            Single command value
        qualifier: c104.Qoc
            Qualifier of command
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> single_cmd = c104.SingleCmd(on=True, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def on(self) -> bool:
        """
        the value
        """
    @property
    def qualifier(self) -> Qoc:
        """
        the command qualifier information
        """
    @property
    def quality(self) -> None:
        """
        This information does not contain quality information.
        """
    @property
    def value(self) -> bool:
        """
        references property ``on``

        The setter is available via point.value=xyz
        """
class SingleInfo(Information):
    """
    This class represents all specific single point information
    """
    def __init__(self, on: bool, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new single info

        Parameters
        ----------
        on: bool
            Single status value
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> single_info = c104.SingleInfo(on=True, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def on(self) -> bool:
        """
        the value
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def value(self) -> bool:
        """
        references property ``on``

        The setter is available via point.value=xyz
        """
class StartEvents:
    """
    This enum contains all StartEvents issue bits to interpret and manipulate protection equipment messages.
    """
    General: typing.ClassVar[StartEvents]
    InEarthCurrent: typing.ClassVar[StartEvents]
    PhaseL1: typing.ClassVar[StartEvents]
    PhaseL2: typing.ClassVar[StartEvents]
    PhaseL3: typing.ClassVar[StartEvents]
    ReverseDirection: typing.ClassVar[StartEvents]
    __members__: typing.ClassVar[dict[str, StartEvents]]
    def is_any(self) -> bool:
        """
        test if there are any bits set
        """
    def is_none(self) -> bool:
        """
        test if no bits are set
        """
    @property
    def value(self) -> int:
        """
        combined bits in integer representation
        """
class Station:
    """
    This class represents local or remote stations and provides access to meta information and containing points
    """
    def add_point(self, io_address: int, type: Type, report_ms: int = 0, related_io_address: int | None = None, related_io_autoreturn: bool = False, command_mode: CommandMode = CommandMode.DIRECT) -> Point | None:
        """
        add a new point to this station and return the new point object

        Parameters
        ----------
        io_address: int
            point information object address (value between 0 and 16777215)
        type: c104.Type
            point information type
        report_ms: int
            automatic reporting interval in milliseconds (monitoring points server-sided only), 0 = disabled
        related_io_address: int, optional
            related monitoring point identified by information object address, that should be auto transmitted on incoming client command (for control points server-sided only)
        related_io_autoreturn: bool
            automatic reporting interval in milliseconds (for control points server-sided only)
        command_mode: c104.CommandMode
            command transmission mode (direct or select-and-execute)

        Returns
        -------
        c104.Station, optional
            station object, if station was added, else None

        Raises
        ------
        ValueError
            io_address or type is invalid
        ValueError
            report_ms, related_io_address or related_auto_return is set, but type is not a monitoring type
        ValueError
            related_auto_return is set, but related_io_address is not set
        ValueError
            related_auto_return is set, but type is not a control type

        Example
        -------
        >>> point_1 = sv_station_1.add_point(common_address=15, type=c104.Type.M_ME_NC_1)
        >>> point_2 = sv_station_1.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
        >>> point_3 = sv_station_1.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=point_2.io_address, related_io_autoreturn=True, command_mode=c104.CommandMode.SELECT_AND_EXECUTE)
        """
    def get_point(self, io_address: int) -> Point | None:
        """
        get a point object via information object address

        Parameters
        ----------
        io_address: int
            point information object address (value between 0 and 16777215)

        Returns
        -------
        c104.Point, optional
            point object, if found, else None

        Example
        -------
        >>> point_11 = my_station.get_point(io_address=11)
        """
    def remove_point(self, io_address: int) -> bool:
        """
        remove an existing point from this station

        Parameters
        ----------
        io_address: int
            point information object address (value between 0 and 16777215)

        Returns
        -------
        bool
            information, if point was removed

        Example
        -------
        >>> sv_station_1.remove_point(io_address=34566)
        """
    def signal_initialized(self, cause: Coi) -> None:
        """
        signal end of initialization for this station to connected clients

        Parameters
        ----------
        cause: c104.Coi
            what caused the (re-)initialization procedure

        Returns
        -------
        None

        Example
        -------
        >>> my_station.signal_initialized(cause=c104.Coi.REMOTE_RESET)
        """
    @property
    def common_address(self) -> int:
        """
        common address of this station (1-65534)
        """
    @property
    def connection(self) -> Connection | None:
        """
        parent Connection of non-local station
        """
    @property
    def has_points(self) -> bool:
        """
        test if station has at least one point
        """
    @property
    def is_local(self) -> bool:
        """
        test if station is local (has sever) or remote (has connection) one
        """
    @property
    def points(self) -> list[Point]:
        """
        list of all Point objects
        """
    @property
    def server(self) -> Server | None:
        """
        parent Server of local station
        """
class StatusAndChanged(Information):
    """
    This class represents all specific packed status point information with change detection
    """
    def __init__(self, status: PackedSingle, changed: PackedSingle, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new event info raised by protection equipment

        Parameters
        ----------
        status: c104.PackedSingle
            Set of current single values
        changed: c104.PackedSingle
            Set of changed single values
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> status_and_changed = c104.StatusAndChanged(status=c104.PackedSingle.I0|c104.PackedSingle.I5, changed=c104.PackedSingle(15), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def changed(self) -> PackedSingle:
        """
        the changed information
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def status(self) -> PackedSingle:
        """
        the current status
        """
    @property
    def value(self) -> PackedSingle:
        """
        references property ``status``

        The setter is available via point.value=xyz
        """
class Step:
    """
    This enum contains all valid IEC60870 step command values to interpret and send step commands.
    """
    HIGHER: typing.ClassVar[Step]
    INVALID_0: typing.ClassVar[Step]
    INVALID_3: typing.ClassVar[Step]
    LOWER: typing.ClassVar[Step]
    __members__: typing.ClassVar[dict[str, Step]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class StepCmd(Information):
    """
    This class represents all specific step command information
    """
    def __init__(self, direction: Step, qualifier: Qoc = Qoc.NONE, recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new step command

        Parameters
        ----------
        direction: c104.Step
            Step command direction value
        qualifier: c104.Qoc
            Qualifier of Command
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> step_cmd = c104.StepCmd(direction=c104.Step.HIGHER, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def direction(self) -> Step:
        """
        the value
        """
    @property
    def qualifier(self) -> Qoc:
        """
        the command qualifier information
        """
    @property
    def quality(self) -> None:
        """
        This information does not contain quality information.
        """
    @property
    def value(self) -> Step:
        """
        references property ``direction``

        The setter is available via point.value=xyz
        """
class StepInfo(Information):
    """
    This class represents all specific step point information
    """
    def __init__(self, position: Int7, transient: bool, quality: Quality = Quality(), recorded_at: datetime.datetime | None = None) -> None:
        """
        create a new step info

        Parameters
        ----------
        position: c104.Int7
            Current transformer step position value
        transient: bool
            Indicator, if transformer is currently in step change procedure
        quality: c104.Quality
            Quality information
        recorded_at: datetime.datetime, optional
            Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

        Example
        -------
        >>> step_info = c104.StepInfo(position=c104.Int7(2), transient=False, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
        """
    @property
    def position(self) -> Int7:
        """
        the value
        """
    @property
    def quality(self) -> Quality:
        """
        the quality

        The setter is available via point.quality=xyz
        """
    @property
    def transient(self) -> bool:
        """
        if the position is transient
        """
    @property
    def value(self) -> Int7:
        """
        references property ``position``

        The setter is available via point.value=xyz
        """
class TlsCipher:
    """
    This enum contains all supported TLS ciphersuites.
    """
    RSA_WITH_NULL_MD5: typing.ClassVar[TlsCipher]
    RSA_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    PSK_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    RSA_WITH_NULL_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_256_CBC_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_CAMELLIA_128_CBC_SHA: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_CAMELLIA_128_CBC_SHA: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_256_CBC_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_CAMELLIA_256_CBC_SHA: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_CAMELLIA_256_CBC_SHA: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    PSK_WITH_NULL_SHA256: typing.ClassVar[TlsCipher]
    PSK_WITH_NULL_SHA384: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_NULL_SHA256: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_NULL_SHA384: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_AES_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_NULL_SHA256: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_NULL_SHA384: typing.ClassVar[TlsCipher]
    RSA_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_CAMELLIA_256_CBC_SHA256: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_AES_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_AES_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_AES_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_AES_128_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_AES_256_CBC_SHA: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_AES_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_AES_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_NULL_SHA: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_NULL_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_NULL_SHA384: typing.ClassVar[TlsCipher]
    RSA_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    RSA_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    PSK_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    PSK_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    PSK_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    PSK_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_ARIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_ARIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_ARIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_ARIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    RSA_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    RSA_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    PSK_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    PSK_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    PSK_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    PSK_WITH_CAMELLIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_128_CCM: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_256_CCM: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_128_CCM: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_256_CCM: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_128_CCM_8: typing.ClassVar[TlsCipher]
    RSA_WITH_AES_256_CCM_8: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_128_CCM_8: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_AES_256_CCM_8: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_128_CCM: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_256_CCM: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_128_CCM: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_256_CCM: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_128_CCM_8: typing.ClassVar[TlsCipher]
    PSK_WITH_AES_256_CCM_8: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_128_CCM_8: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_AES_256_CCM_8: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_128_CCM: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_256_CCM: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_128_CCM_8: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_AES_256_CCM_8: typing.ClassVar[TlsCipher]
    ECJPAKE_WITH_AES_128_CCM_8: typing.ClassVar[TlsCipher]
    ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256: typing.ClassVar[TlsCipher]
    DHE_RSA_WITH_CHACHA20_POLY1305_SHA256: typing.ClassVar[TlsCipher]
    PSK_WITH_CHACHA20_POLY1305_SHA256: typing.ClassVar[TlsCipher]
    ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256: typing.ClassVar[TlsCipher]
    DHE_PSK_WITH_CHACHA20_POLY1305_SHA256: typing.ClassVar[TlsCipher]
    RSA_PSK_WITH_CHACHA20_POLY1305_SHA256: typing.ClassVar[TlsCipher]
    TLS1_3_AES_128_GCM_SHA256: typing.ClassVar[TlsCipher]
    TLS1_3_AES_256_GCM_SHA384: typing.ClassVar[TlsCipher]
    TLS1_3_CHACHA20_POLY1305_SHA256: typing.ClassVar[TlsCipher]
    TLS1_3_AES_128_CCM_SHA256: typing.ClassVar[TlsCipher]
    TLS1_3_AES_128_CCM_8_SHA256: typing.ClassVar[TlsCipher]
    __members__: typing.ClassVar[dict[str, TlsCipher]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class TlsVersion:
    """
    This enum contains all supported TLS versions.
    """
    NOT_SELECTED: typing.ClassVar[TlsVersion]
    SSL_3_0: typing.ClassVar[TlsVersion]
    TLS_1_0: typing.ClassVar[TlsVersion]
    TLS_1_1: typing.ClassVar[TlsVersion]
    TLS_1_2: typing.ClassVar[TlsVersion]
    TLS_1_3: typing.ClassVar[TlsVersion]
    __members__: typing.ClassVar[dict[str, TlsVersion]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class TransportSecurity:
    """
    This class is responsible for configuring transport layer security (TLS) for both servers and clients.
    Once an instance is assigned to a client or server, it becomes read-only and cannot be modified further.
    """
    def __init__(self, validate: bool = True, only_known: bool = True) -> None:
        """
        Create a new transport layer configuration

        Parameters
        ----------
        validate: bool
            validate certificates of communication partners (chain and time)
        only_known: bool
            accept communication only from partners with certificate added to the list of allowed remote certificates

        Example
        -------
        >>> tls = c104.TransportSecurity(validate=True, only_known=False)
        """
    def add_allowed_remote_certificate(self, cert: str) -> None:
        """
        add a trusted communication partners x509 certificate from file

        Parameters
        ----------
        cert: str
            path to trusted communication partners certificate file

        Returns
        -------
        None

        Raises
        ------
        ValueError
            config is readonly and cannot be modified further
        ValueError
            failed to load the certificate file

        Example
        -------
        >>> tls = c104.TransportSecurity(validate=True, only_known=False)
        >>> tls.add_allowed_remote_certificate(cert="certs/client2.crt")
        """
    def set_ca_certificate(self, cert: str) -> None:
        """
        load x509 certificate of trusted authority from file

        Parameters
        ----------
        cert: str
            path to certificate authorities certificate file

        Returns
        -------
        None

        Raises
        ------
        ValueError
            config is readonly and cannot be modified further
        ValueError
            failed to load the certificate file

        Example
        -------
        >>> tls = c104.TransportSecurity(validate=True, only_known=False)
        >>> tls.set_ca_certificate(cert="certs/ca.crt")
        """
    def set_certificate(self, cert: str, key: str, passphrase: str = "") -> None:
        """
        load x509 certificate from file with (optional encrypted) key from file used to encrypt the connection

        Parameters
        ----------
        cert: str
            path to certificate file
        key: bool
            path to certificates private key file
        passphrase: str
            password required to decrypt the certificates private key file

        Returns
        -------
        None

        Raises
        ------
        ValueError
            config is readonly and cannot be modified further
        ValueError
            failed to load the certificate file, the private key file or failed decrypting the private key

        Example
        -------
        >>> tls = c104.TransportSecurity(validate=True, only_known=False)
        >>> tls.set_certificate(cert="certs/server.crt", key="certs/server.key")
        """
    def set_ciphers(self, ciphers: list[TlsCipher]) -> None:
        """
        set the list of accepted TLS cipher suites

        When configuring minimum and maximum TLS versions together with cipher suites, it's crucial to ensure that the selected cipher suites are **compatible** with the specified TLS versions.

        Parameters
        ----------
        ciphers: list[c104.TlsCipher]
            accepted TLS cipher suites

        Returns
        -------
        None

        Raises
        ------
        ValueError
            config is readonly and cannot be modified further
        ValueError
            list is empty or contains invalid cipher suites

        Example
        -------
        >>> tls = c104.TransportSecurity(validate=True, only_known=False)
        >>> tls.set_ciphers(ciphers=[
        >>>   c104.TlsCipher.ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        >>>   c104.TlsCipher.ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        >>>   c104.TlsCipher.ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        >>>   c104.TlsCipher.ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        >>>   c104.TlsCipher.ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
        >>>   c104.TlsCipher.ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        >>>   c104.TlsCipher.DHE_RSA_WITH_AES_128_GCM_SHA256,
        >>>   c104.TlsCipher.DHE_RSA_WITH_AES_256_GCM_SHA384,
        >>>   c104.TlsCipher.DHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        >>>   c104.TlsCipher.TLS1_3_AES_128_GCM_SHA256,
        >>>   c104.TlsCipher.TLS1_3_AES_256_GCM_SHA384,
        >>>   c104.TlsCipher.TLS1_3_CHACHA20_POLY1305_SHA256
        >>> ])
        """
    def set_renegotiation_time(self, interval: datetime.timedelta | None = None) -> None:
        """
        sets the renegotiation interval

        This defines how often the TLS connection should renegotiate. If no interval is
        specified (None), it disables automatic renegotiation.
        Per default renegotiation is disabled.

        Parameters
        ----------
        interval: datetime.timedelta, optional
            The interval as a ``datetime.timedelta`` object. If ``None``, renegotiation is disabled.

        Returns
        -------
        None

        Raises
        ------
        ValueError
            config is readonly and cannot be modified further
        ValueError
            value too small or too large

        Example
        -------
        >>> tls = c104.TransportSecurity(validate=True, only_known=False)
        >>> tls.set_renegotiation_time(interval=datetime.timedelta(minutes=30))
        """
    def set_resumption_interval(self, interval: datetime.timedelta | None = None) -> None:
        """
        sets the session resumption interval for the TLS configuration.

        This interval determines the frequency at which session resumption can occur,
        allowing faster reconnections. If no interval is specified (None), session
        resumption will be disabled.
        Per default session resumption is set to 6 hours.

        Parameters
        ----------
        interval: datetime.timedelta, optional
            The interval as a ``datetime.timedelta`` object. If ``None``, session resumption is disabled.

        Returns
        -------
        None

        Raises
        ------
        ValueError
            config is readonly and cannot be modified further
        ValueError
            value too small or too large

        Example
        -------
        >>> tls = c104.TransportSecurity(validate=True, only_known=False)
        >>> tls.set_resumption_interval(interval=datetime.timedelta(hours=6))
        """
    def set_version(self, min: TlsVersion = ..., max: TlsVersion = ...) -> None:
        """
        sets the supported min and/or max TLS version

        When configuring minimum and maximum TLS versions together with cipher suites, it's crucial to ensure that the selected cipher suites are **compatible** with the specified TLS versions.

        Parameters
        ----------
        min: c104.TlsVersion
            minimum required TLS version for communication
        max: c104.TlsVersion
            maximum allowed TLS version for communication

        Returns
        -------
        None

        Raises
        ------
        ValueError
            config is readonly and cannot be modified further


        Example
        -------
        >>> tls = c104.TransportSecurity(validate=True, only_known=False)
        >>> tls.set_version(min=c104.TLSVersion.TLS_1_2, max=c104.TLSVersion.TLS_1_2)
        """
class Type:
    """
    This enum contains all valid IEC60870 message types to interpret or create points.
    """
    C_BO_NA_1: typing.ClassVar[Type]
    C_BO_TA_1: typing.ClassVar[Type]
    C_CD_NA_1: typing.ClassVar[Type]
    C_CI_NA_1: typing.ClassVar[Type]
    C_CS_NA_1: typing.ClassVar[Type]
    C_DC_NA_1: typing.ClassVar[Type]
    C_DC_TA_1: typing.ClassVar[Type]
    C_IC_NA_1: typing.ClassVar[Type]
    C_RC_NA_1: typing.ClassVar[Type]
    C_RC_TA_1: typing.ClassVar[Type]
    C_RD_NA_1: typing.ClassVar[Type]
    C_RP_NA_1: typing.ClassVar[Type]
    C_SC_NA_1: typing.ClassVar[Type]
    C_SC_TA_1: typing.ClassVar[Type]
    C_SE_NA_1: typing.ClassVar[Type]
    C_SE_NB_1: typing.ClassVar[Type]
    C_SE_NC_1: typing.ClassVar[Type]
    C_SE_TA_1: typing.ClassVar[Type]
    C_SE_TB_1: typing.ClassVar[Type]
    C_SE_TC_1: typing.ClassVar[Type]
    C_TS_NA_1: typing.ClassVar[Type]
    C_TS_TA_1: typing.ClassVar[Type]
    M_BO_NA_1: typing.ClassVar[Type]
    M_BO_TA_1: typing.ClassVar[Type]
    M_BO_TB_1: typing.ClassVar[Type]
    M_DP_NA_1: typing.ClassVar[Type]
    M_DP_TA_1: typing.ClassVar[Type]
    M_DP_TB_1: typing.ClassVar[Type]
    M_EI_NA_1: typing.ClassVar[Type]
    M_EP_TA_1: typing.ClassVar[Type]
    M_EP_TB_1: typing.ClassVar[Type]
    M_EP_TC_1: typing.ClassVar[Type]
    M_EP_TD_1: typing.ClassVar[Type]
    M_EP_TE_1: typing.ClassVar[Type]
    M_EP_TF_1: typing.ClassVar[Type]
    M_IT_NA_1: typing.ClassVar[Type]
    M_IT_TA_1: typing.ClassVar[Type]
    M_IT_TB_1: typing.ClassVar[Type]
    M_ME_NA_1: typing.ClassVar[Type]
    M_ME_NB_1: typing.ClassVar[Type]
    M_ME_NC_1: typing.ClassVar[Type]
    M_ME_ND_1: typing.ClassVar[Type]
    M_ME_TA_1: typing.ClassVar[Type]
    M_ME_TB_1: typing.ClassVar[Type]
    M_ME_TC_1: typing.ClassVar[Type]
    M_ME_TD_1: typing.ClassVar[Type]
    M_ME_TE_1: typing.ClassVar[Type]
    M_ME_TF_1: typing.ClassVar[Type]
    M_PS_NA_1: typing.ClassVar[Type]
    M_SP_NA_1: typing.ClassVar[Type]
    M_SP_TA_1: typing.ClassVar[Type]
    M_SP_TB_1: typing.ClassVar[Type]
    M_ST_NA_1: typing.ClassVar[Type]
    M_ST_TA_1: typing.ClassVar[Type]
    M_ST_TB_1: typing.ClassVar[Type]
    __members__: typing.ClassVar[dict[str, Type]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class UInt16:
    def __init__(self, value: int) -> None:
        """
        create 16 bit fixed-length unsigned integer

        Parameters
        ----------
        value: int
            the value

        Raises
        ------
        ValueError
            cannot convert value to 16 bit unsigned integer
        """
    @property
    def max(self) -> int:
        """
        maximum value
        """
    @property
    def min(self) -> int:
        """
        minimum value
        """
class UInt5:
    def __init__(self, value: int) -> None:
        """
        create 5 bit fixed-length unsigned int

        Parameters
        ----------
        value: int
            the value

        Raises
        ------
        ValueError
            cannot convert value to 5 bit unsigned integer
        """
    @property
    def max(self) -> int:
        """
        maximum value
        """
    @property
    def min(self) -> int:
        """
        minimum value
        """
class UInt7:
    def __init__(self, value: int) -> None:
        """
        create 7 bit fixed-length unsigned int

        Parameters
        ----------
        value: int
            the value

        Raises
        ------
        ValueError
            cannot convert value to 7 bit unsigned integer
        """
    @property
    def max(self) -> int:
        """
        maximum value
        """
    @property
    def min(self) -> int:
        """
        minimum value
        """
class Umc:
    """
    This enum contains all unexpected message cause identifier to interpret error context.
    """
    INVALID_COT: typing.ClassVar[Umc]
    INVALID_TYPE_ID: typing.ClassVar[Umc]
    MISMATCHED_TYPE_ID: typing.ClassVar[Umc]
    NO_ERROR_CAUSE: typing.ClassVar[Umc]
    UNIMPLEMENTED_GROUP: typing.ClassVar[Umc]
    UNKNOWN_CA: typing.ClassVar[Umc]
    UNKNOWN_COT: typing.ClassVar[Umc]
    UNKNOWN_IOA: typing.ClassVar[Umc]
    UNKNOWN_TYPE_ID: typing.ClassVar[Umc]
    __members__: typing.ClassVar[dict[str, Umc]]
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
def disable_debug(mode: Debug) -> None:
    """
    disable debugging modes

    Parameters
    ----------
    mode: c104.Debug
        debug mode bitset

    Example
    -------
    >>> c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection|c104.Debug.Callback|c104.Debug.Gil)
    >>> c104.disable_debug(mode=c104.Debug.Callback|c104.Debug.Gil)
    >>> c104.get_debug_mode() == c104.Debug.Client|c104.Debug.Connection
    """
def enable_debug(mode: Debug) -> None:
    """
    enable additional debugging modes

    Parameters
    ----------
    mode: c104.Debug
        debug mode bitset

    Example
    -------
    >>> c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection)
    >>> c104.enable_debug(mode=c104.Debug.Callback|c104.Debug.Gil)
    >>> c104.get_debug_mode() == c104.Debug.Client|c104.Debug.Connection|c104.Debug.Callback|c104.Debug.Gil
    """
def explain_bytes(apdu: bytes) -> str:
    """
    analyse 104er APDU bytes and convert it into a human readable interpretation

    Parameters
    ----------
    apdu: bytes
        APDU protocol bytes

    Returns
    -------
    str
        information about provided APDU in str representation

    Example
    -------
    >>> def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
    >>>    print("SV] -->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
    """
def explain_bytes_dict(apdu: bytes) -> dict[str, typing.Any]:
    """
    analyse 104er APDU bytes and extract information into a dictionary

    Parameters
    ----------
    apdu: bytes
        APDU protocol bytes

    Returns
    -------
    dict
        information about APDU in dictionary :code:`{"format":str, "type":str, "cot":str, "rx": Optional[int],
        "tx": Optional[int], "numberOfObjects": Optional[int], "sequence": Optional[bool], "negative": Optional[bool],
        "test": Optional[bool], "commonAddress": Optional[int], "originatorAddress": Optional[int],
        "firstInformationObjectAddress": Optional[int], "elements": Optional[str]}`

    Example
    -------
    >>> def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
    >>>    print("SV] -->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes_dict(apdu=data), server.ip, server.port))
    """
def get_debug_mode() -> Debug:
    """
    get current debug mode

    Returns
    ----------
    c104.Debug
        debug mode bitset

    Example
    -------
    >>> mode = c104.get_debug_mode()
    """
def set_debug_mode(mode: Debug) -> None:
    """
    set the debug mode

    Parameters
    ----------
    mode: c104.Debug
        debug mode bitset

    Example
    -------
    >>> c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection)
    """
__version__: str = '2.2.0'
