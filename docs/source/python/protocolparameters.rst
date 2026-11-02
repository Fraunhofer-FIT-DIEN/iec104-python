.. _c104.ProtocolParameters:

ProtocolParameters
###################

.. autoclass:: c104.ProtocolParameters
   :members:
   :exclude-members: __init__,__new__

.. hint::

   Protocol parameters can only be modified while the owning ``c104.Server`` is stopped or the owning ``c104.Connection`` is closed; \
   configure them before calling ``server.start()`` / ``client.start()`` / ``connection.connect()``. \
   Writes while running raise ``RuntimeError``, values outside the IEC 60870-5-104 ranges raise ``ValueError``.

   The window sizes depend on each other (``receive_window_size`` (w) must not exceed ``send_window_size`` (k)), \
   so set ``send_window_size`` first when increasing both and ``receive_window_size`` first when decreasing both.

   Prefer to read and write via the ``connection.protocol_parameters`` or ``server.protocol_parameters`` property instead of storing the instance. \
   The instance is a view onto the parameter block of the underlying lib60870-C and keeps its owner alive as long as it exists.
