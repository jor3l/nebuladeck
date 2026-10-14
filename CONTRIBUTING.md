# Contributing

Issues and PRs welcome. The project is small; keep changes focused.

## Layout

- `firmware/` ESP32-S3 firmware (PlatformIO, Arduino, LovyanGFX). `make build`, `make flash`.
- `host/` Node daemon + `nebuladeck` CLI. `cd host && npm install`.
- `skill/` agent skill (`SKILL.md`) that teaches an agent to use the CLI.
- `docs/PROTOCOL.md` wire protocol between host and device. Update it with any change to either side.
- `tools/demo.py` drives the screen through every state (`make demo`).

## Guidelines

- Firmware: keep the render loop light. Draw into the shared framebuffer, no per-frame allocation.
- Protocol changes are additive: unknown fields and messages must be ignored.
- No new runtime dependencies in `host/` without a good reason.
- Test on a real board when touching `firmware/`; say in the PR what you flashed.
