# folder_hide.py - Jason Theriault 2025
# Simply hides annoying messes in vsCode
# Change what you don't do wanna see
import json
import os
import sys
import re
import shutil

def clean_jsonc(json_str):
    # Remove // comments
    json_str = re.sub(r'//.*', '', json_str)
    # Remove trailing commas (dumb-safe)
    json_str = re.sub(r',\s*([\]}])', r'\1', json_str)
    return json_str

# Detect the platform and set the correct settings.json path
if sys.platform == "win32":
    remote_settings_path = os.path.expanduser(r"~\AppData\Roaming\Code\User\settings.json")
    local_settings_path = os.path.expanduser(r"~\AppData\Roaming\Cursor\User\settings.json")
    SETTINGS_PATH = remote_settings_path if os.path.exists(remote_settings_path) else local_settings_path
elif sys.platform == "linux":
    remote_settings_path = os.path.expanduser(".vscode/settings.json")
    local_settings_path = os.path.expanduser("~/.config/Code/User/settings.json")
    SETTINGS_PATH = remote_settings_path if os.path.exists(remote_settings_path) else local_settings_path
else:
    print("Unsupported OS")
    sys.exit(1)

# Folders and file patterns to toggle
folders_to_toggle = [
    "**/.git",
    "**/node_modules",
    "**/dist",
    "**/.pio",
    "**/.data",
    "**/.debug",
    "**/data",
    "**/debug",
    "**/.doxygen",
    "**/.ci",
    "**/.github",
    "**/SupportingDocs",
    "**/tools",
    "*README*",
    "*.md",
    "*.json",
    "*.py",
    "_*_",
    "*.vscode*",
    ".gulp_stamp",
    "*.build*"
]

# Load the settings file
try:
    with open(SETTINGS_PATH, "r") as f:
        raw = f.read().strip()
        if not raw:
            print(f"Warning: {SETTINGS_PATH} is empty. Initializing as empty object.")
            settings = {}
        else:
            cleaned = clean_jsonc(raw)
            settings = json.loads(cleaned)
except FileNotFoundError:
    print(f"Error: The file {SETTINGS_PATH} does not exist.")
    sys.exit(1)
except json.JSONDecodeError as e:
    print(f"Error: The file {SETTINGS_PATH} contains invalid JSON. {e}")
    sys.exit(1)

# Ensure "files.exclude" exists in settings
if "files.exclude" not in settings:
    settings["files.exclude"] = {}

# Determine the current state of the folders
any_visible = any(
    folder not in settings["files.exclude"] or settings["files.exclude"].get(folder) is False
    for folder in folders_to_toggle
)

# Toggle all folders based on the current state
new_state = any_visible
state_str = 'hidden' if new_state else 'visible'
for folder in folders_to_toggle:
    settings["files.exclude"][folder] = new_state
    print(f"Set visibility of {folder} to {state_str}")

# Backup the original settings file
backup_path = SETTINGS_PATH + ".bak"
try:
    shutil.copy2(SETTINGS_PATH, backup_path)
    print(f"Backup created: {backup_path}")
except Exception as e:
    print(f"Warning: Failed to create backup: {e}")

# Save the updated settings
try:
    with open(SETTINGS_PATH, "w") as f:
        json.dump(settings, f, indent=4)
    print("Settings updated successfully.")
except Exception as e:
    print(f"Error saving settings: {e}")
