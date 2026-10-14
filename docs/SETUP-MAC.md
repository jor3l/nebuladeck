# Setup on macOS

Gets a Mac talking to the NebulaDeck screen and installs the agent skill.
You need: Node 20+, PlatformIO (`pio`), the SC01 Plus, a data-capable USB-C cable.

## 1. Flash the firmware

Always flash the latest `main` first.

1. Put the board in download mode: jumper **BOOT** to **GND** on the DEBUG header, press **RST**.
2. Flash:
   ```bash
   make flash
   ```
3. Remove the jumper and press **RST**. You should see the nebula and "Agent not connected".

`make flash` finds the board by its Espressif USB id (303A), so it never picks another
serial device. Override with `make flash PORT=/dev/cu.usbmodemXXXX`.

Without PlatformIO, use the prebuilt image from the GitHub Releases page instead
(needs `esptool`: `pipx install esptool`). After step 2 below:

```bash
nebuladeck flash            # latest release (or use the web flasher on the project page); or: nebuladeck flash --file nebuladeck-sc01plus.bin
```

## 2. Install the CLI

```bash
make install
```

That runs `npm install` in `host/`, `npm link` (puts `nebuladeck` on your PATH), and links the
skill into `~/.claude/skills/nebuladeck`. Check it:

```bash
which nebuladeck
nebuladeck status
```

If `which` finds nothing, add npm's global bin folder to your PATH:
`export PATH="$(npm prefix -g)/bin:$PATH"` (put it in `~/.zshrc`).

## 3. Try it by hand

The first command starts a small background daemon that owns the USB port and sends the
heartbeat, so you never start anything yourself.

```bash
nebuladeck state working "Testing the screen" --progress 0.4
nebuladeck msg "Hello from the terminal"
nebuladeck ask "Ship it?" Yes No --timeout 60     # tap a button; the answer prints here
nebuladeck done "All good" --tokens-in 48210 --tokens-out 9320
nebuladeck bye                                    # back to the nebula
```

Touch the nebula to make ripples.

Troubleshooting:

| Symptom | Fix |
|---|---|
| `no device connected (skipped)` | Check the cable, run `nebuladeck status` (`open`/`ready`), read `~/.nebuladeck/daemon.log` |
| Daemon stuck or stale | `nebuladeck stop`, then run any command to restart it |
| Wrong port | `export NEBULADECK_PORT=/dev/cu.usbmodemXXXX` before `nebuladeck stop` + a new command |
| Screen falls back to the nebula | Normal: after 60 min with no commands the session ends (`NEBULADECK_IDLE_MIN`, 0 = never) |

## 4. Use it with Claude Code

The skill is installed globally by step 2, so any Claude Code session can use it. Start a new
session (skills load at startup) and ask for something with a few steps, for example:

> Add a README section for the Makefile targets, and use the NebulaDeck screen while you work.

Claude should call `nebuladeck state`, `msg` and finally `done`. Token totals come from
that session's transcript in `~/.claude/projects/`, counted since your last prompt.
`IN` is fresh input plus cache writes; cache reads are left out because they re-send
the same context every turn.

To make Claude use the screen without being asked, add a line to your `~/.claude/CLAUDE.md`:

> For multi-step tasks, use the nebuladeck skill to keep the desk screen updated.

## Notes

- Only one process may hold the serial port; the daemon is that process. Don't run
  `pio device monitor` while it is running (`nebuladeck stop` first).
- Several sessions share the screen; the last command wins.
- Unplugging the board is fine: the daemon reconnects and replays the last state.
