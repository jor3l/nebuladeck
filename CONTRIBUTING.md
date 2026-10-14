# Contributing

Issues and PRs welcome. The project is small; keep changes focused.

## Layout

- `firmware/` ESP32-S3 firmware (PlatformIO, Arduino, LovyanGFX, NimBLE). `make build`, `make flash`, `make monitor`.
- `skill/` agent skill (`SKILL.md`, curl only) and Claude Code hooks (`hooks.json`).
- `site/` setup page: Bluetooth WiFi setup + web flasher (GitHub Pages).
- `docs/PROTOCOL.md` HTTP API, BLE setup service and OTA rules. Update it with any change to either side.
- `tools/demo.sh` drives the screen through every state with curl (`make demo`).

## Tests

Every bug fixed here gets a test, so it stays fixed.

- `make test`: no hardware. `tests/test_hooks.py` runs every hook in `skill/hooks.json` against a fake
  device on localhost; `tests/test_page.py` drives the setup page in headless Chrome with a fake
  NebulaDeck behind Web Bluetooth. CI runs both before any Pages deploy.
- `make e2e`: `tests/test_device.py` against your real device (`NEBULADECK_HOST`, default
  `nebuladeck.local`). It uses its own `E2E-*` agents and cleans up. Run it after flashing.

Python standard library only (plus `curl` and Chrome). On macOS the device tests go through `curl`,
since Local Network privacy can block Python's own sockets.

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
