# toggle_debug.py - Jason Theriault 2025
#    Toggles debug print statements:
#   '// DEBUG_` → `DEBUG_` (enable debugging)
#  `DEBUG_` → `// DEBUG_` (disable debugging)
#  Tracks state using `/* DEBUG_TOGGLE:ENABLED */` or `/* DEBUG_TOGGLE:DISABLED */`
# 

import re
import sys

def toggle_debug(text):

    debug_enabled = "/* DEBUG_TOGGLE:ENABLED */" in text
    debug_disabled = "/* DEBUG_TOGGLE:DISABLED */" in text

    # If no state is found, assume it's enabled by default
    if not debug_enabled and not debug_disabled:
        text += "\n/* DEBUG_TOGGLE:ENABLED */\n"
        debug_enabled = True

    if debug_enabled:
        # Enable debugging → uncomment lines
        text = re.sub(r'(^|\s)// (DEBUG_)', r'\1\2', text)
        text = text.replace("/* DEBUG_TOGGLE:ENABLED */", "/* DEBUG_TOGGLE:DISABLED */")
    else:
        # Disable debugging → comment out lines
        text = re.sub(r'(^|\s)(?<!// )(DEBUG_)', r'\1// \2', text)
        text = text.replace("/* DEBUG_TOGGLE:DISABLED */", "/* DEBUG_TOGGLE:ENABLED */")

    return text

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python toggle_debug.py <file>")
        sys.exit(1)

    file_path = sys.argv[1]

    with open(file_path, "r", encoding="utf-8") as file:
        content = file.read()

    new_content = toggle_debug(content)

    with open(file_path, "w", encoding="utf-8") as file:
        file.write(new_content)

    print(f"Debug toggled in {file_path}")
