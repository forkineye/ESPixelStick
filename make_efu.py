# make_efu.py
# Broke down the java flasher and reverse-engineered the easy way.
# https://github.com/forkineye/ESPSFlashTool
# When you just can't get to them but you need to try someting new

import os
import struct
import subprocess

SIGNATURE = b'EFU\x00'
VERSION = 1

RECORD_TYPE = {
    'sketch': 0x01,   # Matches EFUpdate RecordType::SKETCH_IMAGE
    'spiffs': 0x02    # Matches EFUpdate RecordType::FS_IMAGE
}

def write_record(f_out, record_type, path):
    with open(path, 'rb') as f_in:
        data = f_in.read()
        f_out.write(struct.pack('>H', record_type))      # Type: big-endian 2 bytes
        f_out.write(struct.pack('>I', len(data)))        # Length: big-endian 4 bytes
        f_out.write(data)

def make_efu(sketch_bin, spiffs_bin, output_efu):
    if not os.path.exists(sketch_bin):
        raise FileNotFoundError(f"[EFU ERROR] Sketch binary not found: {sketch_bin}")

    if not os.path.exists(spiffs_bin):
        print(f"[EFU] Filesystem image not found, attempting to build: {spiffs_bin}")
        project_dir = os.path.abspath(os.path.join(sketch_bin, "..", "..", ".."))
        result = subprocess.run(["pio", "run", "-t", "buildfs"], cwd=project_dir)
        if result.returncode != 0 or not os.path.exists(spiffs_bin):
            raise RuntimeError("[EFU ERROR] Failed to build filesystem image.")

    with open(output_efu, 'wb') as f:
        f.write(SIGNATURE)                      # b'EFU\x00'
        f.write(struct.pack('<H', 1))           # Version = 1 (not 256!)

        print(f"[EFU] Adding sketch: {sketch_bin}")
        write_record(f, RECORD_TYPE['sketch'], sketch_bin)

        print(f"[EFU] Adding filesystem: {spiffs_bin}")
        write_record(f, RECORD_TYPE['spiffs'], spiffs_bin)

        print(f"[EFU] Created: {output_efu}")

