# Get CI version number
Import('env')
import os

version = "dev"

if "CI" in os.environ:
    with open("firmware/VERSION", "r") as file:
        version = file.readline().rstrip()
    env.Append(BUILD_FLAGS=[f"-DESPS_VERSION=\"{version}\""])
