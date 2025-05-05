# esps_rebooter.py
# civilized edition - lists, asks, THEN acts

import xml.etree.ElementTree as ET
import requests
import webbrowser

# Your xLights config path
XML_FILE = r"C:\Users\jay\Desktop\Lights 2024\xlights_networks.xml"
COMMON_PATH = "/X6"  # Endpoint you're POSTing to
POST_PAYLOAD = {}    # POST payload if needed

def extract_controller_ips(xml_path):
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()

        ips = []
        for controller in root.findall("Controller"):
            ip = controller.get("IP")
            if ip:
                ips.append(ip)

        return ips

    except ET.ParseError as e:
        print(f"[ERROR] XML parse error: {e}")
    except FileNotFoundError:
        print("[ERROR] File not found.")
    except Exception as e:
        print(f"[ERROR] Unexpected exception: {e}")
    return []

def post_status_to_ips(ip_list, path, payload):
    for ip in ip_list:
        url = f"http://{ip}{path}"
        try:
            response = requests.post(url, json=payload, timeout=3)
            print(f"[OK] POST {url} -> {response.status_code}")
            try:
                print(response.json())
            except ValueError:
                print(response.text)
        except requests.RequestException as e:
            print(f"[FAIL] POST {url} -> {e}")

def open_controllers_in_browser(ip_list):
    print("\n🌐 Opening each controller in your default browser...")
    for ip in ip_list:
        web_url = f"http://{ip}/"
        print(f"🧭 Launching {web_url}")
        webbrowser.open(web_url)

def prompt_yes_no(message):
    while True:
        choice = input(f"{message} (y/n): ").strip().lower()
        if choice in ('y', 'yes'):
            return True
        if choice in ('n', 'no'):
            return False
        print("Please answer with 'y' or 'n'.")

if __name__ == "__main__":
    print("📦 Parsing xLights network config...")
    ips = extract_controller_ips(XML_FILE)
    
    if ips:
        print(f"\n📡 Found {len(ips)} controller IP(s):")
        for idx, ip in enumerate(ips, 1):
            print(f"  {idx}. {ip}")

        if prompt_yes_no("\n🔄 Do you want to POST reset to ALL controllers?"):
            print("\n📨 Sending POST to each controller...\n")
            post_status_to_ips(ips, COMMON_PATH, POST_PAYLOAD)
        else:
            print("\n🚫 Skipped POST reset.")

        if prompt_yes_no("\n🌐 Do you want to open ALL controller web pages?"):
            open_controllers_in_browser(ips)
        else:
            print("\n🚫 Skipped opening web browsers.")

    else:
        print("⚠️ No controller IPs found.")
