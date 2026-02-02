import os
import sys

# On Windows, we need to add the package directory to the DLL search path
# so that the FFmpeg DLLs can be found.
if sys.platform == "win32" and sys.version_info >= (3, 8):
    pkg_dir = os.path.dirname(__file__)
    if os.path.isdir(pkg_dir):
        os.add_dll_directory(pkg_dir)

from ._avioflow import *
