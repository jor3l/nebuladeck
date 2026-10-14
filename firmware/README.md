# firmware

Firmware for the SC01 Plus (ESP32-S3, env `sc01plus`) and SC01 (ESP32, env `sc01`)
(PlatformIO, Arduino, LovyanGFX, NimBLE). `display.h` holds each board's display setup.

- `main.cpp` loop: touch, frame pacing
- `ui.*` screens (welcome/QR, idle, presence, status, ask, done, update) drawn into one PSRAM framebuffer
- `nebula.*` background: two baked, tileable layers scrolled against each other, plus ripples
- `net.*` WiFi credentials, BLE setup service, mDNS
- `api.*` HTTP API on port 80
- `ota.*` update check against GitHub releases and install (`roots.h`: trusted CAs)

`make build`, `make flash`, `make monitor` from the repo root. See [docs/PROTOCOL.md](../docs/PROTOCOL.md).
