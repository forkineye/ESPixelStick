# FullBuild.py - Jason Theriault 2025
# Does a bit of snooping and builds what needs to be built. Define your envs and hit go
# jacked a few things from OG ESPixelStick Scripts, even uses them
# #mybrotherinchrist this has been a savior

# --- IMPORTS --- (no Tariffs)
import os
import sys
import csv
import time
import subprocess
import serial
import psutil
from pathlib import Path
from colorama import Fore, Style
import serial.tools.list_ports
import configparser
from dataclasses import dataclass
from typing import List, Tuple, Optional, Union, Dict


@dataclass
class BuildConfig:
    """Configuration dataclass to hold build parameters"""
    env_name: str
    flash_mode: str = "dio"
    flash_freq: str = "80m"
    baud_rate: int = 460800
    fs_offset: int = 0x3B0000
    fs_size: int = 0x50000
    use_filesystem: bool = True


class ESPBuildManager:
    """Manages the entire ESP32 build and flash process"""
    
    def __init__(self, project_dir: Path):
        """Initialize with project directory"""
        self.project_dir = project_dir
        self.partitions_csv = self.project_dir / "ESP32_partitions.csv"
        self.myenv_txt = self.project_dir / "MyEnv.txt"
        self.build_root = self.project_dir / ".pio" / "build"
        self.gulp_script = self.project_dir / "gulpme.bat"
        self.gulp_stamp = self.project_dir / ".gulp_stamp"
        self.no_fs_stamp = self.project_dir / ".no_filesystem"
        self.esp_tool = self.project_dir / ".pio" / "packages" / "tool-esptoolpy" / "esptool.py"
        
    def should_use_filesystem(self) -> bool:
        """Determine if filesystem should be built and used"""
        if self.no_fs_stamp.exists():
            return False

        if not self.gulp_script.exists():
            print(f"{Fore.YELLOW}⏭ Gulp script not found. No data folder expected. Skipping FS.{Style.RESET_ALL}")
            self.no_fs_stamp.write_text("no_fs")
            return False

        data_dir = self.project_dir / "data"
        if data_dir.exists():
            return True

        print(f"{Fore.YELLOW}⏭ No data/ directory present. Filesystem will be skipped.{Style.RESET_ALL}")
        self.no_fs_stamp.write_text("no_fs")
        return False
    
    def build_all(self, env_name: str) -> None:
        """Build firmware and filesystem for the specified environment"""
        print(f"{Fore.CYAN}🔨 Building firmware for {env_name}...{Style.RESET_ALL}")
        subprocess.run(["platformio", "run", "-e", env_name], check=True)

        if self.should_use_filesystem():
            print(f"{Fore.CYAN}📦 Building filesystem image for {env_name}...{Style.RESET_ALL}")
            subprocess.run(["platformio", "run", "-t", "buildfs", "-e", env_name], check=True)
        else:
            print(f"{Fore.YELLOW}⏭ Skipping filesystem build for {env_name}.{Style.RESET_ALL}")
    
    def has_changed_since_stamp(self, target_path: Path, stamp_path: Path) -> bool:
        """Check if files in target_path were modified since stamp_path was last updated"""
        if not stamp_path.exists():
            return True
            
        stamp_time = stamp_path.stat().st_mtime
        for root, _, files in os.walk(target_path):
            for file in files:
                full_path = Path(root) / file
                if full_path.stat().st_mtime > stamp_time:
                    return True
        return False
    
    def kill_serial_monitors(self) -> None:
        """Close any running serial monitor processes"""
        print(f"{Fore.YELLOW}⚠ Closing any open serial monitor processes...{Style.RESET_ALL}")
        try:
            current_pid = os.getpid()
            for proc in psutil.process_iter(['pid', 'name']):
                name = proc.info['name']
                pid = proc.info['pid']
                if name and pid != current_pid and name.lower() in ("platformio.exe", "platformio-terminal.exe"):
                    subprocess.run(["taskkill", "/f", "/pid", str(pid)], 
                                  stdout=subprocess.DEVNULL, 
                                  stderr=subprocess.DEVNULL)
            print(f"{Fore.GREEN}✔ Any known PlatformIO monitors closed.{Style.RESET_ALL}")
        except Exception as e:
            print(f"{Fore.RED}✘ Could not close serial monitor: {e}{Style.RESET_ALL}")
    
    def find_serial_port(self) -> str:
        """Find an available ESP32 serial port"""
        print("🔍 Searching for available serial ports...")
        ports = serial.tools.list_ports.comports()
        for port in ports:
            if any(chip in port.description for chip in ("USB", "UART", "CP210", "CH340", "ESP32")):
                print(f"{Fore.GREEN}✔ Found port: {port.device}{Style.RESET_ALL}")
                return port.device
        print(f"{Fore.RED}✘ No suitable ESP32 device found. Is it plugged in?{Style.RESET_ALL}")
        sys.exit(1)
    
    def detect_active_environment(self) -> Union[str, List[str]]:
        """Detect available build environments and let user select one"""
        if not self.build_root.exists():
            print(f"{Fore.RED}✘ Build directory not found: {self.build_root}{Style.RESET_ALL}")
            sys.exit(1)

        envs = [d.name for d in self.build_root.iterdir() if d.is_dir()]
        if not envs:
            print(f"{Fore.RED}✘ No environments found in build directory.{Style.RESET_ALL}")
            sys.exit(1)

        if len(envs) == 1:
            return envs[0]

        print(f"{Fore.YELLOW}⚠ Multiple environments detected:{Style.RESET_ALL}")
        for idx, env in enumerate(envs):
            print(f"{Fore.CYAN}[{idx+1}]{Style.RESET_ALL} {env}")

        print(f"\nType the number of the environment to use, or type '{Fore.GREEN}all{Style.RESET_ALL}' to run all.")

        while True:
            choice = input(f"{Fore.MAGENTA}Select environment: {Style.RESET_ALL}").strip().lower()
            if choice == 'all':
                return envs
            if choice.isdigit() and 1 <= int(choice) <= len(envs):
                return envs[int(choice) - 1]
            print(f"{Fore.RED}Invalid choice. Try again.{Style.RESET_ALL}")
    
    def run_gulp_if_needed(self) -> None:
        """Run gulp if data files have changed since last run"""
        if not self.gulp_script.exists():
            print(f"{Fore.YELLOW}⏭ Gulp script not found. Skipping Gulp step.{Style.RESET_ALL}")
            return

        data_dir = self.project_dir / "data"
        if not data_dir.exists():
            print(f"{Fore.YELLOW}⚠ Gulp script exists but data/ directory is missing. Running Gulp anyway...{Style.RESET_ALL}")

        skip_fs_stamp = self.project_dir / ".no_filesystem"
        if skip_fs_stamp.exists():
            print(f"{Fore.YELLOW}⏭ Filesystem was previously marked as unused. Skipping Gulp.{Style.RESET_ALL}")
            return

        has_data = data_dir.exists()

        if not has_data:
            response = input(f"{Fore.MAGENTA}No data/ folder found. Does this environment use a filesystem? [y/N]: {Style.RESET_ALL}").strip().lower()
            if response != 'y':
                print(f"{Fore.YELLOW}⏭ Skipping filesystem setup.{Style.RESET_ALL}")
                with open(skip_fs_stamp, "w") as f:
                    f.write("no_fs")
                return
            else:
                data_dir.mkdir(parents=True)
                print(f"{Fore.GREEN}✔ Created empty data/ folder for filesystem.{Style.RESET_ALL}")

        if self.has_changed_since_stamp(data_dir, self.gulp_stamp):
            print(f"{Fore.CYAN}🛠 Running Gulp: {self.gulp_script}{Style.RESET_ALL}")
            subprocess.run([str(self.gulp_script)], shell=True, check=True)
            with open(self.gulp_stamp, "w") as f:
                f.write(time.strftime("%Y-%m-%d %H:%M:%S"))
            print(f"{Fore.GREEN}✔ Gulp finished and stamp updated.{Style.RESET_ALL}")
        else:
            print(f"{Fore.YELLOW}⏭ data/ unchanged. Skipping Gulp.{Style.RESET_ALL}")
    
    def extract_filesystem_partition(self) -> Tuple[int, int]:
        """Extract filesystem partition offset and size from partitions CSV"""
        if not self.partitions_csv.exists():
            print(f"{Fore.YELLOW}⚠ Partition CSV not found. Using default FS offset 0x3B0000, size 0x50000.{Style.RESET_ALL}")
            return 0x3B0000, 0x50000

        with open(self.partitions_csv, newline='') as csvfile:
            reader = csv.reader(csvfile)
            for row in reader:
                if not row or row[0].strip().startswith("#"):
                    continue
                if len(row) >= 5:
                    name, type_, subtype, offset, size = [x.strip().lower() for x in row[:5]]
                    if (type_ == "data") and (subtype in ("spiffs", "littlefs")):
                        offset_int = int(offset, 0)
                        size_int = int(size, 0)
                        print(f"{Fore.GREEN}✔ Filesystem partition: '{name}', offset=0x{offset_int:X}, size={size_int // 1024} KB{Style.RESET_ALL}")
                        return offset_int, size_int

        print(f"{Fore.YELLOW}⚠ No FS partition found in CSV. Using default FS offset 0x3B0000, size 0x50000.{Style.RESET_ALL}")
        return 0x3B0000, 0x50000
    
    def extract_flash_config(self) -> Tuple[str, str]:
        """Extract flash mode and frequency from the MyEnv.txt file"""
        if not self.myenv_txt.exists():
            print(f"{Fore.RED}✘ MyEnv.txt not found: {self.myenv_txt}{Style.RESET_ALL}")
            sys.exit(1)

        flash_mode = "dio"
        flash_freq = "80m"

        with open(self.myenv_txt, "r") as f:
            for line in f:
                if "'BOARD_FLASH_MODE'" in line:
                    flash_mode = line.split(":")[1].strip().strip("',\"")
                elif "'BOARD_F_FLASH'" in line:
                    raw = line.split(":")[1].strip().strip("',\"").rstrip("L")
                    freq_hz = int(raw)
                    flash_freq = f"{freq_hz // 1000000}m"

        print(f"{Fore.GREEN}✔ Using flash mode: {flash_mode}, flash freq: {flash_freq}{Style.RESET_ALL}")
        return flash_mode, flash_freq
    
    def erase_flash(self, serial_port: str) -> None:
        """Erase the entire flash of the ESP32"""
        if not self.esp_tool.exists():
            print(f"{Fore.RED}✘ esptool.py not found in .pio/packages. Did you run a PlatformIO build first?{Style.RESET_ALL}")
            sys.exit(1)
            
        print(f"{Fore.MAGENTA}💥 Erasing entire flash...{Style.RESET_ALL}")
        subprocess.run([
            sys.executable, str(self.esp_tool),
            "--chip", "esp32",
            "--port", serial_port,
            "erase_flash"
        ], check=True)
    
    def enter_bootloader(self, port: str) -> None:
        """Force the ESP32 into bootloader mode using DTR/RTS signals"""
        print(f"{Fore.CYAN}⏎ Forcing board into bootloader mode on {port}...{Style.RESET_ALL}")
        try:
            with serial.Serial(port, 115200) as ser:
                ser.dtr = False
                ser.rts = True
                time.sleep(0.1)
                ser.dtr = True
                time.sleep(0.1)
                ser.dtr = False
                time.sleep(0.2)
            print(f"{Fore.GREEN}✔ Bootloader mode triggered.{Style.RESET_ALL}")
        except Exception as e:
            print(f"{Fore.RED}✘ Could not trigger bootloader: {e}{Style.RESET_ALL}")
    
    def get_build_files(self, env_name: str) -> Dict[str, Path]:
        """Get all build artifact paths for the specified environment"""
        build_dir = self.build_root / env_name
        return {
            "build_dir": build_dir,
            "bootloader": build_dir / "bootloader.bin",
            "partitions": build_dir / "partitions.bin",
            "firmware": build_dir / "firmware.bin",
            "filesystem": build_dir / "littlefs.bin",
        }
    
    def check_build_artifacts(self, build_files: Dict[str, Path], use_filesystem: bool) -> bool:
        """Check if all required build artifacts exist"""
        required = [build_files["bootloader"], build_files["partitions"], build_files["firmware"]]
        if use_filesystem:
            required.append(build_files["filesystem"])

        missing = [f for f in required if not f.exists()]
        if missing:
            print(f"{Fore.YELLOW}⚠ Missing build artifacts detected:{Style.RESET_ALL}")
            for f in missing:
                print(f"{Fore.YELLOW} - Missing: {f}{Style.RESET_ALL}")
            return False
        else:
            print(f"{Fore.GREEN}✔ All required build artifacts found.{Style.RESET_ALL}")
            return True
    
    def build_if_needed(self, env_name: str, serial_port: str) -> Dict[str, Path]:
        """Check if code has changed and build if necessary"""
        build_files = self.get_build_files(env_name)
        source_dirs = [self.project_dir / "src", self.project_dir / "include"]
        stamp_file = self.project_dir / f".build_stamp_{env_name}"

        needs_build = any(self.has_changed_since_stamp(path, stamp_file) for path in source_dirs)
        
        if needs_build:
            print(f"{Fore.CYAN}🔨 Code changed. Starting full build...{Style.RESET_ALL}")
            self.erase_flash(serial_port)
            self.build_all(env_name)
            with open(stamp_file, "w") as f:
                f.write(time.strftime("%Y-%m-%d %H:%M:%S"))
        else:
            print(f"{Fore.YELLOW}⏭ Code unchanged. Skipping build.{Style.RESET_ALL}")
            
        return build_files
    
    def flash_all_images(self, port: str, config: BuildConfig, build_files: Dict[str, Path], 
                         max_retries: int = 1, retry_delay: int = 10) -> None:
        """Flash all firmware and filesystem images to the ESP32"""
        print(f"{Fore.CYAN}🚀 Flashing all firmware and filesystem images to {port}...{Style.RESET_ALL}")

        cmd = [
            sys.executable,
            str(self.esp_tool),
            "--chip", "esp32",
            "--port", port,
            "--baud", str(config.baud_rate),
            "write_flash",
            "--flash_mode", config.flash_mode,
            "--flash_freq", config.flash_freq,
            "--flash_size", "detect",
            "0x1000", str(build_files["bootloader"]),
            "0x8000", str(build_files["partitions"]),
            "0x10000", str(build_files["firmware"]),
        ]
        
        if config.use_filesystem and build_files["filesystem"].exists():
            cmd.extend([f"0x{config.fs_offset:X}", str(build_files["filesystem"])])
        else:
            print(f"{Fore.YELLOW}⏭ Filesystem image excluded from flash command.{Style.RESET_ALL}")

        attempt = 0
        while attempt <= max_retries:
            try:
                subprocess.run(cmd, check=True)
                print(f"{Fore.GREEN}✅ Full flash complete!{Style.RESET_ALL}")
                return
            except subprocess.CalledProcessError as e:
                print(f"{Fore.RED}✘ Flash attempt {attempt + 1} failed with return code {e.returncode}.{Style.RESET_ALL}")
                if attempt < max_retries:
                    print(f"{Fore.YELLOW}🔁 Retrying in {retry_delay} seconds...{Style.RESET_ALL}")
                    time.sleep(retry_delay)
                    self.enter_bootloader(port)
                else:
                    print(f"{Fore.RED}💥 All flash attempts failed. Giving up.{Style.RESET_ALL}")
                    sys.exit(1)
            attempt += 1
    
    def copy_and_prepare_binaries(self, env_name: str, config: BuildConfig, build_files: Dict[str, Path]) -> None:
        """Copy and prepare binaries for distribution"""
        from shutil import copyfile
        output_dir = self.project_dir / "firmware" / "esp32"
        output_dir.mkdir(parents=True, exist_ok=True)

        env_file = self.project_dir / "MyEnv.txt"
        if not env_file.exists():
            print(f"{Fore.RED}✘ MyEnv.txt not found. Run a PlatformIO build with CustomTargets.py enabled first.{Style.RESET_ALL}")
            return

        board_mcu = "esp32"

        with open(env_file, "r") as f:
            for line in f.readlines():
                if "BOARD_MCU" in line:
                    board_mcu = line.split(":")[1].strip().strip("',\"")

        boot = build_files["bootloader"]
        app = build_files["firmware"]
        part = build_files["partitions"]
        app0 = next((build_files["build_dir"]).glob("*boot_app0.bin"), None)
        fs = build_files["filesystem"] if config.use_filesystem else None

        dst_prefix = output_dir / f"{env_name}"
        paths = {
            "bootloader": (boot, dst_prefix.with_name(dst_prefix.name + "-bootloader.bin")),
            "application": (app, dst_prefix.with_name(dst_prefix.name + "-app.bin")),
            "partitions": (part, dst_prefix.with_name(dst_prefix.name + "-partitions.bin")),
        }
        if fs and fs.exists():
            paths["fs"] = (fs, dst_prefix.with_name(dst_prefix.name + "-littlefs.bin"))
        if app0:
            paths["boot_app0"] = (app0, dst_prefix.with_name(dst_prefix.name + "-boot_app0.bin"))

        for label, item in paths.items():
            src, dst = item
            if src and src.exists():
                print(f"{Fore.GREEN}✔ Copying {label} → {dst.name}{Style.RESET_ALL}")
                copyfile(src, dst)
            else:
                print(f"{Fore.YELLOW}⚠ Missing {label} at {src}{Style.RESET_ALL}")

        merged_bin = dst_prefix.with_name(dst_prefix.name + "-merged.bin")
        esptool_cmd = [
            "esptool", "--chip", board_mcu, "merge_bin",
            "-o", str(merged_bin),
            "--flash_mode", config.flash_mode,
            "--flash_freq", config.flash_freq,
            "--flash_size", "4MB",
            "0x0000", str(paths["bootloader"][1]),
            "0x8000", str(paths["partitions"][1]),
            "0x10000", str(paths["application"][1]),
        ]

        if "boot_app0" in paths:
            esptool_cmd.extend(["0xe000", str(paths["boot_app0"][1])])
        if "fs" in paths:
            esptool_cmd.extend([f"0x{config.fs_offset:X}", str(paths["fs"][1])])
        else:
            print(f"{Fore.YELLOW}⏭ Skipping FS in merged image. No filesystem selected.{Style.RESET_ALL}")

        print(f"{Fore.CYAN}🔧 Generating merged image: {merged_bin.name}{Style.RESET_ALL}")
        subprocess.run(esptool_cmd, check=False)
            
    def launch_serial_monitor(self, port: str, baud: int = 115200) -> None:
        """Launch the PlatformIO serial monitor"""
        print(f"{Fore.CYAN}📡 Launching serial monitor on {port}...{Style.RESET_ALL}")
        try:
            subprocess.run(["platformio", "device", "monitor", "--port", port, "--baud", str(baud)])
        except Exception as e:
            print(f"{Fore.RED}✘ Could not launch serial monitor: {e}{Style.RESET_ALL}")
    
    def process_environment(self, env_name: str, serial_port: str) -> None:
        """Process a single environment - building, flashing and monitoring"""
        print(f"\n{Fore.BLUE}=== Processing environment: {env_name} ==={Style.RESET_ALL}")
        
        # Run gulp if needed
        self.run_gulp_if_needed()
        
        # Build if needed
        build_files = self.build_if_needed(env_name, serial_port)
        
        # Check if filesystem should be used
        use_filesystem = self.should_use_filesystem()
        
        # Check build artifacts
        if not self.check_build_artifacts(build_files, use_filesystem):
            self.build_all(env_name)
            if not self.check_build_artifacts(build_files, use_filesystem):
                print(f"{Fore.RED}✘ Still missing critical build artifacts. Aborting.{Style.RESET_ALL}")
                sys.exit(1)
        
        # Extract flash config & filesystem partition info
        fs_offset, fs_size = self.extract_filesystem_partition()
        flash_mode, flash_freq = self.extract_flash_config()
        
        # Create build config
        config = BuildConfig(
            env_name=env_name,
            flash_mode=flash_mode,
            flash_freq=flash_freq,
            fs_offset=fs_offset,
            fs_size=fs_size,
            use_filesystem=use_filesystem
        )
        
        # Enter bootloader and flash
        self.enter_bootloader(serial_port)
        self.flash_all_images(serial_port, config, build_files, max_retries=1)
        
        # Prepare binary distribution files
        self.copy_and_prepare_binaries(env_name, config, build_files)
        
    def run(self) -> None:
        """Main execution flow"""
        # Close any running serial monitors
        self.kill_serial_monitors()
        
        # Find ESP32 serial port
        serial_port = self.find_serial_port()
        
        # Detect active environment(s)
        env_selection = self.detect_active_environment()
        
        # Process all selected environments
        if isinstance(env_selection, list):
            for env_name in env_selection:
                self.process_environment(env_name, serial_port)
        else:
            self.process_environment(env_selection, serial_port)
            
        # Launch serial monitor after all environments are processed
        self.launch_serial_monitor(serial_port)


# --- MAIN ---
if __name__ == "__main__":
    # Find the project directory 
    project_dir = Path(os.path.join(os.getcwd(), "file_sys.py")).parent
    
    # Create build manager and run
    manager = ESPBuildManager(project_dir)
    manager.run()