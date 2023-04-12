from pathlib import Path
from sphinx.cmd.build import main

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
