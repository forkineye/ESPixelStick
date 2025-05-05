# automation.py - Jason Theriault
# the script that started it all. Merely here for guidance and memories
import subprocess
import sys
import os
from pathlib import Path

def find_platformio_root():
    """Find the PlatformIO project root by looking for platformio.ini."""
    current_dir = os.getcwd()
    while current_dir:
        if os.path.exists(os.path.join(current_dir, "platformio.ini")):
            return current_dir
        parent_dir = os.path.dirname(current_dir)
        if parent_dir == current_dir:  # Stop if we reach the root
            break
        current_dir = parent_dir
    return None

def get_last_used_env():
    """Find the most recently used environment from .pio/build/."""
    build_dir = Path(find_platformio_root()) / ".pio" / "build"
    
    if not build_dir.exists():
        return None

    env_dirs = [d for d in build_dir.iterdir() if d.is_dir()]
    if not env_dirs:
        return None

    # Sort by last modified time
    env_dirs.sort(key=lambda d: d.stat().st_mtime, reverse=True)
    
    return env_dirs[0].name  # Return the most recently used environment

def get_all_envs_from_ini():
    """Fetch all environments from platformio.ini"""
    pio_ini = Path(find_platformio_root()) / "platformio.ini"
    envs = []
    if pio_ini.exists():
        with open(pio_ini, "r") as f:
            lines = f.readlines()
            for line in lines:
                if line.startswith("[env:"):
                    env_name = line.split(":")[1].strip()
                    envs.append(env_name)
    return envs

def get_active_env_from_ini():
    """Fetch active environment from platformio.ini (env_default)."""
    pio_ini = Path(find_platformio_root()) / "platformio.ini"
    if not pio_ini.exists():
        return None

    with open(pio_ini, "r") as f:
        lines = f.readlines()
        for line in lines:
            if line.startswith("env_default"):
                return line.split("=")[-1].strip()
    return None

def run_command(command):
    """Execute a command and print output in real-time."""
    process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    
    for line in process.stdout:
        print(line, end="")

    process.wait()
    if process.returncode != 0:
        print(f"\n❌ Error: Command '{command}' failed with exit code {process.returncode}")
        sys.exit(process.returncode)

def main():
    # Find and switch to PlatformIO root directory
    pio_root = find_platformio_root()
    if not pio_root:
        print("❌ Error: Not a PlatformIO project. No 'platformio.ini' found.")
        sys.exit(1)

    os.chdir(pio_root)
    print(f"📂 Switched to PlatformIO project root: {pio_root}")

    # Ensure PlatformIO is available
    try:
        subprocess.run(["pio", "--version"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    except FileNotFoundError:
        print("❌ Error: PlatformIO is not found in the system PATH.")
        print("➡ Please run this script inside the PlatformIO Terminal in VS Code.")
        sys.exit(1)

    # Check for an explicitly set environment in platformio.ini
    active_env = get_active_env_from_ini()

    # If no environment is set in platformio.ini, use the last used environment
    if not active_env:
        print("❌ No active environment found in platformio.ini. Falling back to the most recently used environment.")
        active_env = get_last_used_env()

    # If there's no environment from both sources, use the first available one from platformio.ini
    if not active_env:
        all_envs = get_all_envs_from_ini()
        if all_envs:
            active_env = all_envs[0]  # Choose the first available environment
        else:
            print("❌ No environment found in platformio.ini or .pio/build/.")
            sys.exit(1)

    print(f"\n🔹 Using active environment: {active_env}")

    # Define PlatformIO commands with the detected environment
    commands = [
        "gulpme.bat",  # Run gulp (if available)
        f"pio run -e {active_env} -t erase",  # Erase flash
        f"pio run -e {active_env} -t buildfs",  # Build filesystem
        f"pio run -e {active_env} -t upload",  # Upload firmware
        f"pio run -e {active_env} -t uploadfs"  # Upload filesystem

    ]

    for cmd in commands:
        print(f"\n🚀 Executing: {cmd}")
        run_command(cmd)

    print("\n✅ Flashing and uploading completed successfully.")
    sys.exit(0)

# Run the script standalone
if __name__ == "__main__":
    main()
