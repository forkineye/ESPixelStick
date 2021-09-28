# Update firmware.json files
import os, json

version = "dev"
RELEASE = "ESPS_RELEASE"

with open("dist/firmware/VERSION", "r") as file:
    version = file.readline().rstrip()
    file.close

with open("dist/firmware/firmware.json", "r+") as file:
    data = json.load(file)
    if RELEASE in os.environ:
        data["release"] = f"ESPixelStick {version}"
    else:
        data["release"] = f"ESPixelStick {version} (untested)"
    file.seek(0)
    json.dump(data, file)
    file.truncate()