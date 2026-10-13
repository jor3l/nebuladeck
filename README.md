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
   │  MCP (stdio)
   ▼
host/  MCP server (Node/TS)  ──USB CDC serial, newline-delimited JSON──▶  firmware/ (ESP32-S3, LVGL)
   ▲                                                                       │ touch events
   └───────────────────────────────────────────────────────────────────────┘
```

- `firmware/` PlatformIO + Arduino/ESP-IDF, LVGL, LovyanGFX display/touch drivers
- `host/` MCP server: owns the serial port, reconnects, heartbeat, tool surface
- `skill/` the agent skill describing when/how to use the screen
- `docs/PROTOCOL.md` wire protocol and MCP tools (source of truth)

## Device states

| State | Trigger | Screen |
|---|---|---|
| Idle | no host heartbeat for 3 s | nebula animation + "Agent not connected" |
| Connected | host heartbeat | status UI (project, state, messages, actions) |

## Status

Scaffolding. Next: firmware bring-up (display + touch + nebula), then protocol,
then MCP server, then skill.
