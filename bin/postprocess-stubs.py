#!/usr/bin/env python3

from pathlib import Path
import re


def replace_property_pattern(filename):
    # Read the entire file content as a single string
    with open(filename, "r") as f:
        content = f.read()


    # Define a multiline regex pattern
    empty_static_pattern = re.compile(
        r'''
    @staticmethod
    def _([^(]+)_\(\*args,\s\*\*kwargs\):
        ...''',
        re.MULTILINE | re.DOTALL  # Enable multiline and verbose mode for readability
    )

    # Define a multiline regex pattern
    property_pattern = re.compile(
        r'''
    @property
    def ([^(]+)\(\*args,\s\*\*kwargs\):
        """
        ([^:\n]+):([^"]+?)
        """
    @\1.setter
    def \1\(\*args,\s\*\*kwargs\):
        ...''',
        re.MULTILINE | re.DOTALL  # Enable multiline and verbose mode for readability
    )

    # Define a multiline regex pattern
    property_readonly_pattern = re.compile(
        r'''
    @property
    def ([^(]+)\(\*args,\s\*\*kwargs\):
        """
        ([^:\n]+):([^"]+?)
        """''',
        re.MULTILINE | re.DOTALL  # Enable multiline and verbose mode for readability
    )

    empty_line = re.compile(r'^([\t ]+)$', re.MULTILINE)
    method_line = re.compile(r'^([\t ]+)def (.+)$', re.MULTILINE)

    test = r'''
    @property
    def value(*args, **kwargs):
        """
        c104.Byte32: references property ``blob`` (read-only)

        The setter is available via point.value=xyz
        """
    @property
    def cot(*args, **kwargs):
        """
        c104.Cot: cause of transmission (read-only)
        """
'''
    for match in property_readonly_pattern.finditer(test):
        print("Property Name:", match.group(1))
        print("Return Type:", match.group(2))
        print("Docstring:", match.group(3))
        print("----------------")

    # Replace with the desired pattern
    def replacement_property(match):
        getter_name = match.group(1).strip()  # Extract the property name
        original_return_type = match.group(2).strip()  # Extract the return type
        docstring = match.group(3).strip()  # Extract and clean up the docstring

        return_type = original_return_type.replace("c104.", "") # strip namespace prefix

        print("update property", match.group(0))

        return (
            f'''
    @property
    def {getter_name}(self) -> {return_type}:
        """
        {docstring}
        """
    @{getter_name}.setter
    def {getter_name}(self, value: {return_type}) -> None:
        """
        set {docstring}

        Parameters
        ----------
        value: {original_return_type}
            new value for {getter_name}

        Returns
        -------
        None
        """'''
        )

    # Replace with the desired pattern
    def replacement_property_readonly(match):
        property_name = match.group(1).strip()  # Extract the property name
        return_type = match.group(2).strip()  # Extract the return type
        docstring = match.group(3).strip()  # Extract and clean up the docstring

        return_type = return_type.replace("c104.", "") # strip namespace prefix
        docstring = docstring.replace(" (read-only)", "")

        print("update readonly property", property_name)

        return (
            f'''
    @property
    def {property_name}(self) -> {return_type}:
        """
        {docstring}
        """'''
        )

    # Replace with the desired pattern
    def replace_namespace(match):
        indent = match.group(1)
        definition = match.group(2).replace("c104.", "").strip()

        return f'{indent}def {definition}'

    modified_content = content

    # Perform the replacement
    modified_content = empty_static_pattern.sub(r'', modified_content)
    modified_content = property_pattern.sub(replacement_property, modified_content)
    modified_content = property_readonly_pattern.sub(replacement_property_readonly, modified_content)
    modified_content = empty_line.sub(r'', modified_content)
    modified_content = method_line.sub(replace_namespace, modified_content)

    # Write the modified content back to the file
    with open(filename, "w") as f:
        f.write(modified_content)


# path variables
PROJECT_DIR = Path(__file__).parent.parent
BUILD_DIR = PROJECT_DIR / "stubs/c104"

# Replace with the path to your .pyi stub file
stub_file = BUILD_DIR / "__init__.pyi"
replace_property_pattern(stub_file)
