import platform
import shutil
import os
Import("env")

f = open("./MyEnv.txt", "a")
f.write(env.Dump())
f.close()

PACKAGES_DIR = env['PROJECT_PACKAGES_DIR']
FRAMEWORK_DIR = os.path.join(PACKAGES_DIR, "framework-arduinoespressif8266/libraries")
SD_DIR = os.path.join(FRAMEWORK_DIR, "SD")
SDFAT_DIR = os.path.join(FRAMEWORK_DIR, "ESP8266SdFat")
# print("PACKAGES_DIR " + PACKAGES_DIR)
# print("FRAMEWORK_DIR " + FRAMEWORK_DIR)
# print("SD_DIR " + SD_DIR)

if os.path.exists(SD_DIR):
    shutil.rmtree(SD_DIR)

if os.path.exists(SDFAT_DIR):
    shutil.rmtree(SDFAT_DIR)

def before_build(target, source, env):
    print("BeforeBuild")

env.AddPreAction("buildprog", before_build)
