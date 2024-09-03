.. _c104.Type:

Type
####

.. autoclass:: c104.Type
   :members:

Float, Short, NaN
-----------------

**M_ME_NC_1**, **M_ME_TF_1**, **C_SE_NC_1**, **C_SE_TC_1**, **P_ME_NC_1** use the IEEE 754 standard for floating point numbers with support for *NaN* (Not a Number).
Note that you must test for NaN for point values of these types!
Since this connector supports the protocol in its present form, it is possible to send an **M_ME_NC_1** message with a quality of *GOOD* but with a value of *NaN*.
