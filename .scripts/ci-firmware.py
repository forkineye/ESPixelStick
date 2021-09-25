# Update firmware.json files
import os, json

version = "dev"
RELEASE = "ESPS_RELEASE"

with open("dist/firmware/VERSION", "r") as file:
    version = file.readline().rstrip()
    file.close

if RELEASE in os.environ:
    with open("dist/firmware.json", "r+") as file:
        data = json.load(file)
        data["release"] = f"ESPixelStick {version}"
        file.seek(0)
        json.dump(data, file)
        file.truncate()
else:
    with open(".ci/firmware.json", "r+") as file:
        data = json.load(file)
        data["release"] = f"ESPixelStick CI Build #{version}"
        file.seek(0)
        json.dump(data, file)
        file.truncate()