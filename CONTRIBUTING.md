# Contributing

Issues and PRs welcome. The project is small; keep changes focused.

## Layout

- `firmware/` ESP32-S3 firmware (PlatformIO, Arduino, LovyanGFX, NimBLE). `make build`, `make flash`, `make monitor`.
- `skill/` agent skill (`SKILL.md`, curl only) and Claude Code hooks (`hooks.json`).
- `site/` setup page: Bluetooth WiFi setup + web flasher (GitHub Pages).
- `docs/PROTOCOL.md` HTTP API, BLE setup service and OTA rules. Update it with any change to either side.
- `tools/demo.sh` drives the screen through every state with curl (`make demo`).

## Guidelines

- Firmware: keep the render loop light. Draw into the shared framebuffer, no per-frame allocation.
- Protocol changes are additive: unknown fields and endpoints must be ignored.
- Nothing may leave the local network except the OTA check to GitHub. Keep it that way.
- Agents need nothing but `curl`. Keep it that way: new features go on the device, not into host tools.
- Test on a real board when touching `firmware/`; say in the PR what you flashed.

## Releases

Every push to `main` publishes the rolling `dev` prerelease (dev devices update to it). To ship to
everyone, run the **release** workflow with a version `X.Y.Z`: it tags `vX.Y.Z`, marks it latest and
redeploys the web flasher.
