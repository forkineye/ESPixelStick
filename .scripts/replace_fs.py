Import("env")
import platform

os_name = platform.system().lower()

mkfsPath = "./dist/bin/"
if "windows" == os_name:
    mkfsPath += "win32/mklittlefs.exe"
elif "linux" == os_name:
    mkfsPath += "linux64/mklittlefs"
elif "linux64" == os_name:
    mkfsPath += "linux64/mklittlefs"
elif "macos" == os_name:
    mkfsPath += "macos/mklittlefs"
else:
    print("ERROR: Could not determine OS type. Got: " + str(os_name))

print("Replace MKSPIFFSTOOL with " + mkfsPath)
env.Replace (MKSPIFFSTOOL = mkfsPath)
