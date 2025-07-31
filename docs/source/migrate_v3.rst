
Create a point
""""""""""""""""

Replace

>>> step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1, report_ms=2000)

with

>>> step_point = sv_station_2.add_point(io_address=31, info=c104.StepInfo(1), report_ms=2000)

You need to specify a type instead? Use c104.Information.from_type(type: c104.Type)

>>> step_point = sv_station_2.add_point(io_address=31, info=c104.Information.from_type(c104.Type.M_ST_TB_1), report_ms=2000)

keep in mind that this will still allow the module to decide if it should use a timestamped value or not


Get type of a point
""""""""""""""""""""

test all usages of point.type, it will now only return non-timestamped c104.Type identifiers, if you need the timestamped version use pont.info.as_type(true)
since one point can now use both messages with and without timestamp there cannot be a fixed setting


Transmit a point manually
""""""""""""""""""""""""""

If required, add the timestamp=True argument to transmit a point with timestamp

>>> point.transmit(cause=c104.Cot.SPONTANEOUS, timestamp=True)
