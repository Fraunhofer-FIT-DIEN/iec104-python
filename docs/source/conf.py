# Configuration file for the Sphinx documentation builder.
import subprocess, os

read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'

if read_the_docs_build:
    subprocess.call('cd ../../; doxygen ./Doxyfile', shell=True)


# -- Project information -----------------------------------------------------

project = "iec104-python"
copyright = "2020-2024, Fraunhofer Institute for Applied Information Technology FIT"
author = "Martin Unkel <martin.unkel@fit.fraunhofer.de>"

release = "1.0"
version = "1.17.0"


# -- General configuration ---------------------------------------------------

extensions = [
    'sphinx.ext.duration',
    'sphinx.ext.doctest',
    'sphinx.ext.autodoc',
    'sphinx.ext.autosummary',
    'sphinx.ext.napoleon',
    'breathe',
    'sphinx_autodoc_typehints',
    'sphinx.ext.intersphinx'
]

intersphinx_mapping = {
    'python': ('https://docs.python.org/3/', None),
    'sphinx': ('https://www.sphinx-doc.org/en/master/', None),
    'c104': ('https://iec104-python.readthedocs.io/', 'objects.inv'),
}
intersphinx_disabled_domains = ['std']

templates_path = ['_templates']

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_rtd_theme'

# -- Options for autodoc -----------------------------------------------------

autoclass_content = "class"

autodoc_class_signature = "separated"

autodoc_member_order = "bysource"

autodoc_default_options = {
    'members': True,
    'member-order': 'groupwise',
    'special-members': '__init__',
    'undoc-members': True,
    'inherited-members': True,
    'exclude-members': '__weakref__,__str__,__repr__,__new__'
}

# -- Options for breathe -----------------------------------------------------

breathe_projects = {
    "iec104-python": "../build/xml/",
}

breathe_default_project = "iec104-python"

typehints_fully_qualified = True
always_document_param_types = True
typehints_use_signature = True
typehints_use_signature_return = True
