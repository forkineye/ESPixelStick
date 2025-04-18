import os

Import("env")

PROJECT_DIR = env['PROJECT_DIR']
REPO_NAME   = "esp-eth-drivers"
REPO_URL    = "https://github.com/espressif"
LIBRARY_DIR = PROJECT_DIR + "/Libraries" + "/" + REPO_NAME

print ("PROJECT_DIR:" + PROJECT_DIR)
print ("LIBRARY_DIR:" + LIBRARY_DIR)
print ("  REPO_NAME:" + REPO_NAME)
print ("   REPO_URL:" + REPO_URL)

if not os.path.exists(LIBRARY_DIR):
    print ("Clone " + REPO_NAME + " Libray")
    env.Execute("git clone --depth 100 " + REPO_URL + "/" + REPO_NAME + " " + LIBRARY_DIR )
else:
    print ("Pull " + REPO_NAME + " Libray")
    env.Execute("git --work-tree=" + LIBRARY_DIR + " --git-dir=" + LIBRARY_DIR + "/.git pull origin master --depth 100")
