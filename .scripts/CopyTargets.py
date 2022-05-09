import platform
import shutil
import os
Import("env")

BUILD_DIR = env['PROJECT_BUILD_DIR']
PIOENV    = env['PIOENV']
BOARD     = env['BOARD']
PROGNAME  = env['PROGNAME']
BOARD_MCU = env['BOARD_MCU']

# print("BUILD_DIR " + BUILD_DIR)
# print("PIOENV " + PIOENV)
# print("BOARD " + BOARD)
# print("PROGNAME " + PROGNAME)
# print("BOARD_MCU " + BOARD_MCU)
# print("CustomTargets.py - Success")

BOARD_FLASH_MODE = env['BOARD_FLASH_MODE']
BOARD_F_FLASH = env['BOARD_F_FLASH'].removesuffix('000000L') + 'm'
# print("BOARD_FLASH_MODE " + BOARD_FLASH_MODE)
# print("BOARD_F_FLASH " + BOARD_F_FLASH)

SRC_DIR  = BUILD_DIR + "/" + PIOENV + "/"
SRC_BIN  = SRC_DIR + PROGNAME + ".bin"
SRC_PART = SRC_DIR + "partitions.bin"
SRC_DBG  = SRC_DIR + PROGNAME + ".elf"

# print("SRC_BIN " + SRC_BIN)

DST_DIR   = "./firmware/" + BOARD_MCU + "/"
DST_BIN   = DST_DIR + PIOENV + "-app.bin"
DST_PART  = DST_DIR + PIOENV + "-partitions.bin"
DST_BOOT  = DST_DIR + PIOENV + "-bootloader.bin"

DBG_DIR   = "./debug/" + BOARD_MCU + "/"
DST_DBG   = DBG_DIR + PIOENV + ".elf"

def after_build(source, target, env):

    pathfrm = os.path.join("", DST_DIR)
    print("mkdirs: path - '" + pathfrm + "'")
    shutil.rmtree(pathfrm, True)
    os.makedirs(pathfrm, 0x7777, True)
    os.system("chmod -R 0x777 ./firmware")
    print("Listing dir: " + pathfrm)
    os.system("ls -al ./firmware")
    print("Copy from: '" + SRC_BIN + "' to '" + DST_BIN + "'")
    shutil.copyfile(SRC_BIN, DST_BIN)
    print("Listing dir: " + pathfrm)
    os.system("ls -al ./firmware")

    pathdbg = os.path.join("", DBG_DIR)
    print("mkdirs: pathdbg - '" + pathdbg + "'")
    shutil.rmtree(pathdbg, True)
    os.makedirs(pathdbg, 0x777, True)
    os.system("chmod -R 0x777 ./firmware")
    print("Listing dir: " + pathdbg)
    os.system("ls -al ./debug")
    print("Copy from: '" + SRC_DBG + "' to '" + DST_DBG + "'")
    shutil.copyfile(SRC_DBG, DST_DBG)
    print("Listing dir: " + pathdbg)
    os.system("ls -al ./debug")

    if("FLASH_EXTRA_IMAGES" in env):
        FLASH_EXTRA_IMAGES = env['FLASH_EXTRA_IMAGES']
        # print('FLASH_EXTRA_IMAGES: ')
        for imageId in range(len(FLASH_EXTRA_IMAGES)):
            ImagePath = FLASH_EXTRA_IMAGES[imageId][1]
            # print(ImagePath)
            if("boot_app0" in ImagePath):
                print("Copy: " + ImagePath)
                shutil.copyfile(ImagePath, DST_DIR + "boot_app0.bin")

            elif ("partitions" in ImagePath):
                print("Copy: " + ImagePath)
                shutil.copyfile(ImagePath, DST_PART)

            elif("bootloader" in ImagePath):
                SRC_BL_DIR = ImagePath.replace('${BOARD_FLASH_MODE}', BOARD_FLASH_MODE).replace("${__get_board_f_flash(__env__)}", BOARD_F_FLASH)
                # print("Copy SRC_BL_DIR: " + SRC_BL_DIR)
                print("Copy: " + SRC_BL_DIR)
                shutil.copyfile(SRC_BL_DIR, DST_BOOT)

env.AddPostAction("buildprog", after_build)
