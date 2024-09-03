Welcome to iec104-python's documentation!
================================================

Introduction
------------

This software provides an object-oriented high-level python module to simulate scada systems and remote terminal units communicating via 60870-5-104 protocol.

The python module c104 combines the use of lib60870-C with state structures and python callback handlers.

Examples
--------

Remote terminal unit
^^^^^^^^^^^^^^^^^^^^^^^^^^

  .. code-block:: python

        import c104

        # server and station preparation
        server = c104.Server(ip="0.0.0.0", port=2404)

        # add local station and points
        station = server.add_station(common_address=47)
        measurement_point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
        command_point = station.add_point(io_address=12, type=c104.Type.C_RC_TA_1)

        server.start()


Scada unit
^^^^^^^^^^^^

  .. code-block:: python

        import c104

        client = c104.Client(tick_rate_ms=1000, command_timeout_ms=5000)

        # add RTU with station and points
        connection = client.add_connection(ip="127.0.0.1", port=2404, init=c104.Init.INTERROGATION)
        station = connection.add_station(common_address=47)
        measurement_point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
        command_point = station.add_point(io_address=12, type=c104.Type.C_RC_TA_1)

        client.start()

Visit https://github.com/Fraunhofer-FIT-DIEN/iec104-python/tree/main/examples for more examples.

.. toctree::
   :maxdepth: 1
   :caption: Contents:

   install
   changelog
   python/index
   core/index
