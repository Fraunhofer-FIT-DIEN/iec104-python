import sys
from types import BuiltinFunctionType
from c104 import _c104

# Iterate over all attributes in the submodule
for attr_name, attr_value in vars(_c104).items():
    # Check if the attribute is a class or a function
    if isinstance(attr_value, (type, BuiltinFunctionType)):
        # Change the __module__ attribute
        attr_value.__module__ = 'c104'
        # print("set", attr_value)
        # Set the attribute in the main module namespace
        setattr(sys.modules['c104'], attr_name, attr_value)

__all__ = [attr_name for attr_name in vars(_c104) if isinstance(getattr(_c104, attr_name), type)]
