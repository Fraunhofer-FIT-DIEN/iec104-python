#!/usr/bin/env python3

from pathlib import Path
try:
    from sphinx.cmd.build import main
except ImportError:
    import sys
    print('Please install requirements first:\npython3 -m pip install -r docs/requirements.txt')
    sys.exit(1)


# path variables
PROJECT_DIR = Path(__file__).parent.parent
BUILD_DIR = PROJECT_DIR / "docs/build"

# sphinx-build arguments
argv = [
    "-b",
    "html",
    "-a",
    str(PROJECT_DIR / "docs/source"),
    str(BUILD_DIR)
]

main(argv)
