# Board: sc01plus (default, ESP32-S3) or sc01 (original WT32-SC01, ESP32). E.g. make flash BOARD=sc01
BOARD ?= sc01plus
# The port is auto-detected by USB id, so other serial devices (webcams...) are never picked:
# the SC01 Plus's own USB (Espressif 303A), or the SC01's USB-serial chip (CP210x 10C4, CH340 1A86).
# Override with: make flash PORT=/dev/cu.usbmodemXXXX
ifeq ($(BOARD),sc01)
USBID := 10C4|1A86
else
USBID := 303A
endif
PORT ?= $(shell pio device list 2>/dev/null | awk '/^\/dev\//{p=$$0} /VID:PID=($(USBID))/{print p; exit}')
FW   := firmware

.PHONY: build flash monitor run clean port demo install image test e2e

build:
	cd $(FW) && pio run -e $(BOARD)

port:
	@echo "$(PORT)"

# SC01 Plus: usually flashes as is; if it can't connect, jumper BOOT to GND on the DEBUG header,
# press RST, flash, remove the jumper, press RST. SC01: its USB-serial chip resets it by itself.
flash:
	@test -n "$(PORT)" || { echo "No $(BOARD) found on USB. SC01 Plus: try download mode (BOOT-GND, press RST)."; exit 1; }
	cd $(FW) && pio run -e $(BOARD) -t upload --upload-port $(PORT)

monitor:
	@test -n "$(PORT)" || { echo "No $(BOARD) found on USB."; exit 1; }
	cd $(FW) && pio device monitor -p $(PORT)

run: flash monitor

clean:
	cd $(FW) && pio run -e $(BOARD) -t clean

# Cycles the screen through every state with curl (over WiFi).
demo:
	./tools/demo.sh

# Agent skill for Claude Code (no CLI: agents call the device's HTTP API with curl).
install:
	mkdir -p $$HOME/.claude/skills
	ln -sfn "$$(pwd)/skill" $$HOME/.claude/skills/nebuladeck
	@echo "Installed the skill. Try: curl -s http://nebuladeck.local/api/info"

# Flashable images + OTA app images for both boards (dist/), like the CI release assets.
# Local builds are channel "local" (no OTA checks) unless PLATFORMIO_BUILD_FLAGS says otherwise.
image:
	cd $(FW) && pio run -e sc01plus -e sc01
	for b in sc01plus sc01; do ESPTOOL="$$(head -1 $$(which pio) | sed 's/^#!//') $$HOME/.platformio/packages/tool-esptoolpy/esptool.py" ./scripts/merge-firmware.sh $$b; done

# Tests without hardware: Claude Code hooks against a fake device, the setup page in headless Chrome.
test:
	python3 tests/test_hooks.py
	python3 tests/test_page.py

# End-to-end against the real device (NEBULADECK_HOST, default nebuladeck.local). Uses its own
# "E2E-*" agents and cleans up; flash the build you want to test first.
e2e:
	python3 tests/test_device.py
