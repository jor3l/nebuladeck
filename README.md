# NebulaDeck

An always-on external display for AI agents, built on the **Panlee SC01 Plus**
(ESP32-S3, 3.5" 320×480 capacitive touch, run vertically).

An agent (e.g. Claude Code with a skill) pushes project state and messages to
the screen through a small CLI that talks to the device over USB serial. You walk away; the screen
stays on and tells you what the agent is doing. Touch lets you answer back
(approve, pick an option, dismiss).

When no agent is connected the device shows a slow nebula animation and
"Agent not connected".

## Goals

- Modern, sleek, fully customizable UI driven by data, not reflashing
- Fast and light: LVGL on the ESP32-S3, partial redraws, no network stack needed
- One cable: power + data over USB
- Agent-friendly: a tiny shell CLI (`nebuladeck`) plus a skill that teaches the agent to use it; works with any agent that has a shell

## Architecture

```
Agent (Claude Code + skill)
   │  shell: nebuladeck state/msg/ask/done
   ▼
host/  daemon + CLI (Node)  ──USB CDC serial, newline-delimited JSON──▶  firmware/ (ESP32-S3, LVGL)
   ▲                                                                       │ touch events
   └───────────────────────────────────────────────────────────────────────┘
```

- `firmware/` PlatformIO + Arduino, LovyanGFX, custom renderer
- `host/` daemon (owns the serial port, heartbeat, reconnect) + `nebuladeck` CLI
- `skill/` the agent skill describing when/how to use the screen
- `docs/PROTOCOL.md` wire protocol and CLI commands (source of truth)

## Device states

| State | Trigger | Screen |
|---|---|---|
| Idle | no host heartbeat for 3 s | nebula animation + "Agent not connected" |
| Connected | host heartbeat | status UI (project, state, messages, actions) |

## Hardware

- **Panlee / Wireless-Tag SC01 Plus** (ESP32-S3, 3.5" 320x480 capacitive touch, ST7796 + FT6336). Also sold as WT32-SC01 Plus.
- A USB-C **data** cable. No drivers needed on macOS: the board is a standard USB serial device.
- The board has only a reset button. To flash it, jumper **BOOT** to **GND** on the DEBUG header
  and press **RST** (download mode), then remove the jumper and press RST again.

## Quick start

**Web flasher** (Chrome/Edge, no tools needed): https://jor3l.github.io/nebuladeck/ (live once the repo is public and a release is tagged).

See [docs/SETUP-MAC.md](docs/SETUP-MAC.md). In short: flash the firmware (`make flash` from source,
or `nebuladeck flash` with a prebuilt release image), `make install`, then
`nebuladeck state working "Hello"`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). MIT licensed.

## Status

Firmware (nebula, status UI, ask, completed screen with token card), protocol, daemon + CLI
(`host/`) and a Claude Code skill (`skill/`) work. The firmware renders with LovyanGFX,
not LVGL. Next: real-world testing.
