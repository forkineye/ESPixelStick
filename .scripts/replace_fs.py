import platform
Import ("env")

OS_NAME = platform.system().lower()
FS_PATH = "./dist/bin/"

if OS_NAME == "windows" :
    FS_PATH += "win32/mklittlefs.exe"
elif OS_NAME == "linux" :
    FS_PATH += "linux64/mklittlefs"
elif OS_NAME == "linux64" :
    FS_PATH += "linux64/mklittlefs"
elif OS_NAME == "darwin" :
    FS_PATH += "macos/mklittlefs"
else:
    print("ERROR: Could not determine OS type. Got: " + str (OS_NAME))

print("Replace MKSPIFFSTOOL with " + FS_PATH)
env.Replace (MKSPIFFSTOOL = FS_PATH)
