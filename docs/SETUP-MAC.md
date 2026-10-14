# Setup on macOS

Gets the NebulaDeck screen on your WiFi and lets Claude Code use it.
You need: an SC01 Plus or SC01, a USB data cable, and Chrome or Edge (for flashing and Bluetooth setup).
PlatformIO (`pio`) only if you build the firmware yourself. Nothing gets installed for the agent
except one skill file: it talks to the screen with `curl`.

## 1. Flash the firmware

Easiest: open [jor3l.github.io/nebuladeck](https://jor3l.github.io/nebuladeck/) in Chrome or Edge,
choose **Flash device**, pick **Latest** (stable) or **Dev** (newest build of `main`), and follow the steps.

From source:

1. SC01 Plus: if flashing can't connect, put it in download mode: jumper **BOOT** to **GND** on the
   DEBUG header, press **RST**. The SC01 needs nothing.
2. `make flash` (SC01 Plus) or `make flash BOARD=sc01`
3. If you used the jumper, remove it and press **RST**.

`make flash` finds the board by its USB id (Espressif 303A for the SC01 Plus, CP210x or CH340 for
the SC01), so it never picks another serial device. Override with `make flash PORT=/dev/cu.usbmodemXXXX`. Local builds are channel
`local`: they never offer OTA updates.

You only flash over USB once. Released firmware updates itself over WiFi: when a newer
build of its channel exists, the screen offers **Update** / **Skip**. You can also tap the logo →
**Check updates**, or `curl -4 -X POST http://nebuladeck.local/api/update`.

## 2. Connect it to WiFi

After flashing, the screen shows **Welcome** and a QR code.

1. Scan the QR code with your phone, or open the setup page on your computer and choose
   **Connect to WiFi**. Chrome/Edge on Android, macOS, Windows or Linux work; on iPhone use the
   Bluefy browser (Safari has no Web Bluetooth).
2. **Connect over Bluetooth** and pick `NebulaDeck-XXXX` (XXXX matches the end of the QR link).
3. Choose your network, type the password, **Connect**. The screen shows progress.
4. The page shows the device's address. A few seconds later the screen switches to
   "Agent not connected" with `nebuladeck.local` and its IP.

The ESP32 only joins **2.4 GHz** networks. To change network later, tap the logo → **Forget WiFi**,
or hold the screen for 5 seconds. The old network is kept until a new one works.

Check it from the Mac:

```bash
curl -4 -s http://nebuladeck.local/api/info
```

If `nebuladeck.local` doesn't resolve (some routers block mDNS), use the IP shown on the screen and
`export NEBULADECK_HOST=<ip>` in `~/.zshrc`. The skill, hooks and demo all honour it.

## 3. Install the skill

On the setup page, **Set up your agent** has a one-line install command. Or, from a clone:

```bash
make install        # links skill/ into ~/.claude/skills/nebuladeck
```

## 4. Try it by hand

```bash
H=http://nebuladeck.local/api; J='Content-Type: application/json'
curl -4 -s -H "$J" -d '{"status":"working","title":"Testing the screen","progress":0.4}' $H/state
curl -4 -s -H "$J" -d '{"text":"Hello from the terminal"}' $H/msg
curl -4 -s -H "$J" -d '{"id":"t1","prompt":"Ship it?","options":["Yes","No"],"timeout":60}' $H/ask
curl -4 -s "$H/answer?id=t1"        # tap a button, then run again: answered:Yes
curl -4 -s -H "$J" -d '{"summary":"All good","tokens_in":48210,"tokens_out":9320}' $H/done
curl -4 -s -X POST $H/bye             # back to the nebula
```

Touch the nebula to make ripples; tap the logo for device info. `make demo` walks through every screen.

Troubleshooting:

| Symptom | Fix |
|---|---|
| `curl: (6) Could not resolve host: nebuladeck.local` | Use the IP shown on the screen: `export NEBULADECK_HOST=<ip>` |
| Times out | Mac and screen on different networks/VLANs (guest WiFi, client isolation). Put them on the same LAN |
| Screen stuck on "Connecting to WiFi" | Router off or password changed: hold the screen 5 s and set it up again |
| Screen falls back to the nebula | Normal: 30 min without requests ends the session |
| Serial logs | `make monitor` (IP, OTA and setup messages) |

## 5. Use it with Claude Code

The skill is global, so any Claude Code session can use it. Start a new session (skills load at
startup) and ask for something with a few steps, for example:

> Add a README section for the Makefile targets, and use the NebulaDeck screen while you work.

Claude should post `state`, `msg` and finally `done`. Questions arrive as buttons on the screen.

To make Claude use the screen without being asked, add a line to your `~/.claude/CLAUDE.md`:

> For multi-step tasks, use the nebuladeck skill to keep the desk screen updated.

## 6. Show presence while chatting (hooks)

The skill covers tasks. To show "Claude connected" with live secondary text ("Thinking...",
"Running a command...", "Needs you", "Ready") even in a plain chat, install the hooks in
`skill/hooks.json`. Easiest: ask Claude

> Install the NebulaDeck hooks from the nebuladeck skill's hooks.json into my ~/.claude/settings.json.

or merge the `hooks` entries yourself. Restart Claude Code sessions afterwards. Each hook is a
background `curl` that sends only the event name (and the tool name), so it never slows a session
down and never sends your prompts or files. To remove them, delete the entries whose command
calls `/api/hook`. An explicit task from the skill takes over the screen while it runs.

## Notes

- Several sessions (or computers) can share the screen; the last request wins.
- Rebooting or unplugging the screen is fine: it rejoins WiFi and the next request redraws it.
