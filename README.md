# NebulaDeck

An always-on external display for AI agents, built on the **Panlee SC01 Plus**
(ESP32-S3, 3.5" 320×480 capacitive touch, run vertically).

An agent (e.g. Claude Code with a skill) pushes project state and messages to
the screen over USB serial, through an MCP server. You walk away; the screen
stays on and tells you what the agent is doing. Touch lets you answer back
(approve, pick an option, dismiss).

When no agent is connected the device shows a slow nebula animation and
"Agent not connected".

## Goals

- Modern, sleek, fully customizable UI driven by data, not reflashing
- Fast and light: LVGL on the ESP32-S3, partial redraws, no network stack needed
- One cable: power + data over USB
- Agent-friendly: small, well-typed MCP tool surface plus a skill that teaches it

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
- `docs/PROTOCOL.md` wire protocol and MCP tools (source of truth)

## Device states

| State | Trigger | Screen |
|---|---|---|
| Idle | no host heartbeat for 3 s | nebula animation + "Agent not connected" |
| Connected | host heartbeat | status UI (project, state, messages, actions) |

## Quick start

See [docs/SETUP-MAC.md](docs/SETUP-MAC.md). In short: `make flash`, `make install`, then
`nebuladeck state working "Hello"`.

## Status

Firmware (nebula, status UI, ask, completed screen with token card), protocol, daemon + CLI
(`host/`) and a Claude Code skill (`skill/`) work. The firmware renders with LovyanGFX,
not LVGL. Next: real-world testing; an MCP wrapper over the daemon is optional.
