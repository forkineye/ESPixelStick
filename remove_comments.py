# toggle_comments.py - Jason Theriault 2025
# Sometimes, it's just too much and you end up nuking your help
# SERIOUSLY FUCKS with documentation (Doxy)
# #dowhatyouwant but this shouldn't be used 
import re

def remove_comments(text):
    # Remove single-line comments (//)
    text = re.sub(r'//.*', '', text)
    # Remove multi-line comments (/* ... */)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    return text

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python remove_comments.py <file>")
        sys.exit(1)

    file_path = sys.argv[1]
    with open(file_path, 'r') as file:
        content = file.read()

    new_content = remove_comments(content)

    with open(file_path, 'w') as file:
        file.write(new_content)