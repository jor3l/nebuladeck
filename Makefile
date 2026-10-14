# Port is auto-detected by Espressif's USB id (303A), so the Anker webcam's
# serial port is never picked. Override with: make flash PORT=/dev/cu.usbmodemXXXX
PORT ?= $(shell pio device list 2>/dev/null | awk '/^\/dev\//{p=$$0} /VID:PID=303A/{print p; exit}')
FW   := firmware

.PHONY: build flash monitor run clean port demo install image

build:
	cd $(FW) && pio run -e sc01plus

port:
	@echo "$(PORT)"

# Needs download mode first: jumper BOOT to GND on the DEBUG header, press RST.
# Remove the jumper and press RST again afterwards.
flash:
	@test -n "$(PORT)" || { echo "No Espressif device found. Put the board in download mode (BOOT-GND, press RST)."; exit 1; }
	cd $(FW) && pio run -e sc01plus -t upload --upload-port $(PORT)

monitor:
	@test -n "$(PORT)" || { echo "No Espressif device found."; exit 1; }
	cd $(FW) && pio device monitor -p $(PORT)

run: flash monitor

clean:
	cd $(FW) && pio run -e sc01plus -t clean

# Cycles the screen through every state with curl (over WiFi).
demo:
	./tools/demo.sh

# Agent skill for Claude Code (no CLI: agents call the device's HTTP API with curl).
install:
	mkdir -p $$HOME/.claude/skills
	ln -sfn "$$(pwd)/skill" $$HOME/.claude/skills/nebuladeck
	@echo "Installed the skill. Try: curl -s http://nebuladeck.local/api/info"

# Flashable image (dist/nebuladeck-sc01plus.bin) + OTA app image, like the CI release assets.
# Local builds are channel "local" (no OTA checks) unless PLATFORMIO_BUILD_FLAGS says otherwise.
image: build
	ESPTOOL="$$(head -1 $$(which pio) | sed 's/^#!//') $$HOME/.platformio/packages/tool-esptoolpy/esptool.py" ./scripts/merge-firmware.sh
