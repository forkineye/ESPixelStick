# post_efu.py
Import("env")
import os
import sys
import subprocess
from datetime import datetime
from SCons.Script import AlwaysBuild
from make_efu import make_efu
from pathlib import Path
import serial.tools.list_ports
from colorama import Fore, Style

build_dir = env.subst("$PROJECT_BUILD_DIR")
project_dir = env.subst("$PROJECT_DIR")
output_dir = os.path.join(project_dir, "firmware", "EFU")
os.makedirs(output_dir, exist_ok=True)

def detect_active_environment():
    """Detect available build environments and let user select one"""
    build_root = Path(build_dir)
    current_variant = env.subst("$PIOENV")
    
    if not build_root.exists():
        print(f"{Fore.RED}✘ Build directory not found: {build_root}{Style.RESET_ALL}")
        return current_variant
        
    envs = [d.name for d in build_root.iterdir() if d.is_dir()]
    if not envs:
        print(f"{Fore.RED}✘ No environments found in build directory.{Style.RESET_ALL}")
        return current_variant
        
    if len(envs) == 1:
        return envs[0]
        
    print(f"{Fore.YELLOW}⚠ Multiple environments detected:{Style.RESET_ALL}")
    for idx, env_name in enumerate(envs):
        print(f"{Fore.CYAN}[{idx+1}]{Style.RESET_ALL} {env_name}")
        
    print(f"\nType the number of the environment to use, or press enter to use current ({current_variant}).")
    
    while True:
        choice = input(f"{Fore.MAGENTA}Select environment: {Style.RESET_ALL}").strip().lower()
        if not choice:
            return current_variant
        if choice.isdigit() and 1 <= int(choice) <= len(envs):
            return envs[int(choice) - 1]
        print(f"{Fore.RED}Invalid choice. Try again.{Style.RESET_ALL}")

def find_serial_port():
    """Find an available ESP32 serial port"""
    print("🔍 Searching for available serial ports...")
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if any(chip in port.description for chip in ("USB", "UART", "CP210", "CH340", "ESP32")):
            print(f"{Fore.GREEN}✔ Found port: {port.device}{Style.RESET_ALL}")
            return port.device
    print(f"{Fore.RED}✘ No suitable ESP32 device found. Is it plugged in?{Style.RESET_ALL}")
    return None

def after_build(source, target, env):
    selected_variant = detect_active_environment()
    
    sketch_bin = os.path.join(build_dir, selected_variant, "firmware.bin")
    fs_bin = os.path.join(build_dir, selected_variant, "littlefs.bin")
    efu_out = os.path.join(build_dir, selected_variant, f"{selected_variant}.efu")
    
    if not os.path.exists(sketch_bin):
        print(f"[EFU ERROR] Sketch binary not found for {selected_variant}.")
        return

    if not os.path.exists(fs_bin):
        print(f"[EFU] Filesystem image not found for {selected_variant}, building...")
        subprocess.run(["pio", "run", "-t", "buildfs", "-e", selected_variant], cwd=project_dir)

    if not os.path.exists(fs_bin):
        print("[EFU ERROR] Filesystem still missing after build attempt.")
        return

    print(f"[EFU] Creating EFU: {os.path.basename(efu_out)}")
    make_efu(sketch_bin, fs_bin, efu_out)

    timestamp = datetime.now().strftime('%Y%m%d')
    final_filename = f"{selected_variant}_{timestamp}.efu"
    final_path = os.path.join(output_dir, final_filename)

    efu_tool = os.path.join(project_dir, "efu_tool.py")
    result = subprocess.run([
        sys.executable, efu_tool,
        "--efu", efu_out,
        "--project", project_dir,
        "--env", selected_variant,
        "--output", output_dir
    ])

    if result.returncode != 0:
        print("[EFU TOOL] ❌ EFU validation failed!")
    else:
        print(f"[EFU TOOL] ✅ EFU validated and saved to {final_path}")
        
    flash_device = input(f"{Fore.MAGENTA}Flash the device with this EFU? (y/N): {Style.RESET_ALL}").strip().lower()
    if flash_device == 'y':
        serial_port = find_serial_port()
        if serial_port:
            print(f"{Fore.GREEN}✔ Would flash to {serial_port} (implement flashing code here){Style.RESET_ALL}")

AlwaysBuild(env.Alias("post_efu", "buildprog", after_build))
