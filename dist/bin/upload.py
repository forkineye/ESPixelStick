#!/usr/bin/env python3

# Wrapper to call esptool, stripped down from the Arduino ESP8266 project.

import sys
import os
import tempfile

sys.argv.pop(0) # Remove executable name
toolspath = os.path.dirname(os.path.realpath(__file__)).replace('\\', '/') # CWD in UNIX format
try:
    sys.path.insert(0, toolspath + "/pyserial") # Add pyserial dir to search path
    sys.path.insert(0, toolspath + "/esptool") # Add esptool dir to search path
    import esptool # If this fails, we can't continue and will bomb below
except:
    sys.stderr.write("pyserial or esptool directories not found next to this upload.py tool.\n")
    sys.exit(1)

esptool.main(sys.argv)
