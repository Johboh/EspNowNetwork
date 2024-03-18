import os
import sys
import argparse
import requests
import hashlib

parser = argparse.ArgumentParser(description='Upload firmware to HTTP storage')

parser.add_argument('-u', '--base_url', required=True, help="URL on where to upload the firmware.bin and the firmware_version.txt")
parser.add_argument('-f', '--firmware_version', help="The firmware version for the binary")
parser.add_argument('-F', '--firmware_version_file', help="The file containing the firmware version for the binary")
parser.add_argument('firmware', help="Path to the firmware.bin that should be uploaded")

args = parser.parse_args()

base_url = args.base_url
firmware = args.firmware
firmware_version = args.firmware_version
firmware_version_file = args.firmware_version_file

if not os.path.isfile(firmware):
    sys.exit("Firmare file %s does not exists." % firmware)

if firmware_version is not None and firmware_version_file is not None:
    sys.exit("Must pass only firmare version OR firmware version file, not both.")

if firmware_version_file is not None and not os.path.isfile(firmware_version_file):
    sys.exit("Firmare version file %s does not exists." % firmware_version_file)

files = {'file': ('firmware.bin', open(firmware,'rb').read())}
requests.post(base_url, files=files)

if firmware_version is None:
    firmware_version = open(firmware_version_file, 'r').read()

if len(firmware_version) == 0:
    sys.exit("Firmware version is empty.")

files = {'file': ('firmware_version.txt', firmware_version)}
requests.post(base_url, files=files)

md5 = hashlib.md5(open(firmware,'rb').read()).hexdigest()
files = {'file': ('firmware.md5', md5)}
requests.post(base_url, files=files)

print("Uploaded %s as firmware.bin with firmware version %s and MD5 %s to %s" % (firmware, firmware_version, md5, base_url))