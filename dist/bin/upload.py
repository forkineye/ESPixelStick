#!/usr/bin/env python3
import os
import sys
import runpy

toolspath = os.path.dirname(os.path.realpath(__file__))

sys.path.insert(0, os.path.join(toolspath, "libs"))
sys.path.insert(0, toolspath)

try:
    runpy.run_module("esptool", run_name="__main__")
except Exception as e:
    sys.stderr.write(f"Upload failed: {e}\n")
    sys.exit(1)
