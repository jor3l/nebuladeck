#!/usr/bin/env python3
"""Writes the web flasher manifest for one channel (CH=latest|dev, NAME=<release name>).

ESP Web Tools picks the build matching the connected chip, so one button serves both boards:
ESP32-S3 -> SC01 Plus, ESP32 -> SC01. Boards whose image isn't in site/<CH>/ are left out.
"""
import json, os, sys

ch, name = os.environ["CH"], os.environ["NAME"]
here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "site", ch)
builds = []
for chip, board in (("ESP32-S3", "sc01plus"), ("ESP32", "sc01")):
    if os.path.exists(os.path.join(here, "nebuladeck-%s.bin" % board)):
        builds.append({"chipFamily": chip, "parts": [{"path": "%s/nebuladeck-%s.bin" % (ch, board), "offset": 0}]})
if not builds:
    sys.exit("no images in site/%s" % ch)
json.dump({"name": "NebulaDeck", "version": name, "new_install_prompt_erase": False, "builds": builds}, sys.stdout, indent=2)
