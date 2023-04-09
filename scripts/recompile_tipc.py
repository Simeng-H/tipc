#!/usr/bin/env python3
import os
import subprocess
import sys

ROOT_DIR = '/Users/simeng/local_dev/Compilers/project/tipc-mem'
BUILD_DIR = os.path.join(ROOT_DIR, 'build')

if __name__ == '__main__':
    # original_uid = 501
    # print("Original UID: {}".format(original_uid))
    # os.setuid(0)
    subprocess.run(["rm", "-rf", BUILD_DIR])

    # os.setuid(original_uid)
    subprocess.run(["mkdir", BUILD_DIR])
    subprocess.run(["cmake", ".."], cwd=BUILD_DIR)
    subprocess.run(["make", "-j6"], cwd=BUILD_DIR)
