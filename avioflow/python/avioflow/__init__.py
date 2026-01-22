import os
import sys

# On Windows, we need to explicitly add the package directory to the DLL search path
# so that the C++ extension can find its dependencies (avioflow.dll, ffmpeg dlls).
if os.name == 'nt':
    # Add our own directory to the search path
    pkg_dir = os.path.dirname(os.path.abspath(__file__))
    os.add_dll_directory(pkg_dir)

# Import all symbols from the binary extension
from ._avioflow import *

# Explicitly export symbols for better help() and IDE support
__all__ = [
    "AudioStreamOptions", 
    "DeviceInfo", 
    "Metadata", 
    "AudioSamples", 
    "AudioDecoder", 
    "DeviceManager", 
    "set_log_level"
]

# Workaround for Python's help() trying to use 'cat' or 'less' on some Windows environments
if os.name == 'nt' and 'PAGER' not in os.environ:
    os.environ['PAGER'] = 'more'
