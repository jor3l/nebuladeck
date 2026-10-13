# Port is auto-detected by Espressif's USB id (303A), so the Anker webcam's
# serial port is never picked. Override with: make flash PORT=/dev/cu.usbmodemXXXX
PORT ?= $(shell pio device list 2>/dev/null | awk '/^\/dev\//{p=$$0} /VID:PID=303A/{print p; exit}')
FW   := firmware

.PHONY: build flash monitor run clean port

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
