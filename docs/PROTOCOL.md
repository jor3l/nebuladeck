# Protocol

The device has two interfaces: a Bluetooth LE service, used only during WiFi setup, and an
HTTP JSON API on the local network for everything else. Both sides ignore unknown fields.

## WiFi setup (Bluetooth LE)

Active while the welcome/QR screen is shown: on first boot, after `POST /api/wifi/forget`, or after
holding the screen for 5 seconds. The device advertises as `NebulaDeck-XXXX` (last MAC bytes; the QR
code links to `https://jor3l.github.io/nebuladeck/?setup=XXXX`).

Service `6e656275-6c61-6465-636b-000000000001` ("nebuladeck" in hex):

| Characteristic | UUID suffix | Props | Value (UTF-8 JSON) |
|---|---|---|---|
| networks | `…0002` | read | `[[ssid, rssi, secured], ...]` strongest first, deduplicated, at most 512 bytes |
| control | `…0003` | write | `{"cmd":"scan"}` or `{"cmd":"join","ssid":"…","pass":"…"}` |
| status | `…0004` | read, notify | `{"state", "ssid"?, "ip"?, "host"?, "error"?, "saved"?, "fw"}` |

`state`: `idle` → `scanning` → `ready` (networks readable) → `connecting` → `connected` (with
`ip`, `host`) or `failed` (with `error`). Notifications are cut at the MTU, so treat one as "changed"
and read the characteristic for the full value.

The device tries the network for up to 20 s. Credentials are saved only after it joins, so a typo
never replaces a working network. About 6 s after `connected` it turns Bluetooth off and moves to the
idle screen. In setup with a saved network, **Cancel** on screen goes back to it.

## HTTP API

`http://nebuladeck.local` (mDNS; services `_http._tcp` and `_nebuladeck._tcp`) on port 80. Requests
and replies are JSON. Send `Content-Type: application/json`. Every reply has `ok`, and errors also
have `error`.

### Several agents

The device keeps up to 10 agents, each identified by `agent` + `project` (e.g. "Claude" in
"nebuladeck"), with its own status, task, activity history (12 entries), question and summary.
Send both in every request. A request with a known `agent` but no `project` goes to the most
recently active agent of that name. An agent is dropped after 30 minutes without requests or
on `bye`; with all 10 slots taken, the least recently seen one is replaced. Swiping a card sideways
(either way) removes it (and cancels its question); the agent's next request brings it back.

Agents survive restarts (power loss, flashing, updates): the device saves them to flash at most
every 5 s and restores them at boot, questions included. With no clock it can't tell how long it
was off, so restored cards read RECONNECTING until their agent sends something, and expire after
10 minutes if it never does. `POST /api/restart` saves and restarts.

With one agent the screens are the single-agent ones below. With two or more, the nebula shows
one card per agent (logo, name and project, what it is doing, its latest activity, progress);
the list scrolls when it doesn't fit. Cards needing you come first, then busy agents, then the rest; a card's label reads QUESTION,
NEEDS YOU, DONE, WORKING (a task), ACTIVE (busy from hooks) or READY. A question or "needs you" turns that agent's card amber
instead of taking over the screen; tapping a card opens that agent's screens, and "All agents"
returns to the list. `GET /api/info` lists the agents.

Any POST below (except `bye`) starts or extends that agent's session. Any body may include
`agent` (≤32 chars, e.g. `"Claude"`, `"Codex"`, `"Gemini"`, `"Cursor"`, `"Copilot"`): the screen shows that
name, with the agent's logo when it recognises it (Claude/Anthropic, Codex/OpenAI/ChatGPT/GPT,
Gemini/Google, Cursor, Copilot) and a green dot otherwise. `/api/hook` defaults it to Claude. With no request for 30 minutes
the screen returns to the idle nebula ("Agent not connected"), and a new session starts clean.

| Method + path | Body | Effect |
|---|---|---|
| `POST /api/state` | `project`, `status` (`idle\|working\|waiting\|done\|error`), `title`, `progress?` (0-1) | Set the main status |
| `POST /api/msg` | `text`, `level?` (`info\|warn\|error`), `ttl?` (s) | Push a message line |
| `POST /api/presence` | `agent?`, `text`, `attn?`, `new_turn?`, `end_turn?` | Connected with no task: nebula screen with "<agent> connected" and `text` as the secondary line (a trailing `...` animates; `attn` turns the dot amber). `new_turn` clears a finished/stale screen; `end_turn` closes a task card the agent left open |
| `POST /api/done` | `summary`, `tokens_in`, `tokens_out`, `duration?` (s) | Task finished: nebula with the completion message and a token card. Stays until the next `state`/`ask`/`bye` |
| `POST /api/ask` | `id?` (≤32 chars, random if absent), `prompt`, `options[]` (1-4), `timeout?` (s, 0 = none) | Show buttons; replies `{"id"}`. A newer ask replaces an open one |
| `GET /api/ask?id=ID` | | `{"state": "pending\|answered\|cancelled", "choice"?, "label"?}`. Unknown or replaced ids read as `cancelled` |
| `GET /api/answer?id=ID` | | Same as plain text for shell loops: `pending`, `cancelled` or `answered:<label>` |
| `POST /api/ask_cancel` | `id?` | Remove the open ask (all, or only if `id` matches) |
| `POST /api/clear` | | Clear messages |
| `POST /api/ping` | | Keep the session alive |
| `POST /api/hook?event=E&tool=T&kind=K&project=P&sub=S&role=R&agent=A` | none | Claude Code hook event, mapped to `presence`/`bye` on the device (see Hooks) |
| `POST /api/bye` | | End the session: back to the nebula now |
| `GET /api/info` | | `fw`, `name`, `host`, `ip`, `ssid`, `rssi`, `w`, `h`, `session`, `update?`, `agents` (in card order: `agent`, `project`, `label`, `line`, `status`, `asking`, `attn`, `completed`, `activity`, `last`), `max_agents`, and diagnostics: `uptime_s`, `heap`, `wifi_sleep`, `wifi_drops`, `silent_clients`, `requests`, `frame_ms`, `frame_max_ms`, `log` |
| `POST /api/update` | | Install the newer release in `update` (409 if none). The device shows progress and reboots |
| `POST /api/wifi/forget` | | Erase the saved network and show the setup QR |

When an ask ends, the screen returns to what it showed before (a question asked outside a
task goes back to the presence screen). An ask also ends when its `timeout` passes, when `done`/`bye` arrives, or when the session expires.
It is answered only by a tap on the screen.

```bash
curl -4 -s -H 'Content-Type: application/json' -d '{"status":"working","title":"Hello"}' http://nebuladeck.local/api/state
```

## Firmware updates (OTA)

Builds carry a channel (`FW_CHANNEL`) and version (`FW_VERSION`), set by CI:

| Channel | Version | Checks | Update when |
|---|---|---|---|
| `stable` | `X.Y.Z` | `GET api.github.com/repos/jor3l/nebuladeck/releases/latest` | `tag_name` is a higher semver |
| `dev` | `dev-<run>-<sha>` | `…/releases/tags/dev` (rolling prerelease) | its `name` has a higher run number |
| `local` | `local` | never | |

The device checks shortly after joining WiFi and then every 12 h, over HTTPS verified against the
roots in `firmware/src/roots.h`. If a newer build exists, it offers **Update** / **Skip** on the
nebula screens. **Skip** hides that version for good. **Update** (or `POST /api/update`) downloads the
release asset for its board, `nebuladeck-sc01plus-ota.bin` or `nebuladeck-sc01-ota.bin` (app only), into the
spare OTA slot and reboots. `GET /api/info` reports `board` and `channel`.

## Hooks

`skill/hooks.json` holds Claude Code hooks for presence during plain chats. Each one runs
`curl -4 -s -m 2 -X POST "http://${NEBULADECK_HOST:-nebuladeck.local}/api/hook?event=<Event>" &` in the
background, so a missing screen never slows Claude down. The hook's stdin (which can hold whole files)
is not sent. For `PreToolUse`, `sed` pulls out only `tool_name` and sends it as `tool`.

| Event | Screen |
|---|---|
| `SessionStart` | "Ready", clears a finished screen |
| `UserPromptSubmit` | "Thinking...", clears a finished screen |
| `PreToolUse` | By tool: "Running a command...", "Reading...", "Editing...", "Searching...", "Browsing...", "Delegating...", "Planning...", else "Working..." |
| `Notification` | "Needs you" (amber) for permission prompts; with `kind=idle` (Claude Code's "waiting for your input" reminder) a calm "Waiting for you" |
| `SubagentStart` / `SubagentStop` | A subagent card appears / goes (see below) |
| `Stop` | "Ready", closes a task card left open |
| `SessionEnd` | `bye` |

Subagents: events fired inside a subagent carry Claude Code's `agent_id` / `agent_type`, which the
hooks send as `sub` / `role`. Each running subagent is a compact row inside its session's card:
its icon, its type ("Explore", "general-purpose") and a dot in its status colour; a subagent needing
you turns the whole card amber. The session's card is created if a subagent shows up first.
`/api/info` lists each session followed by its subagents. `SubagentStart` creates
it, `SubagentStop` removes it, and so does the session's `SessionEnd` (or any `bye` for the session).
A subagent card without events for 10 minutes is dropped. While a session has subagent cards, its own
`Stop` and idle reminder read "An agent is working..." / "N agents working..." (ACTIVE) instead of
"Ready" / "Waiting for you". Subagents run in worktrees (`<project>/.claude/worktrees/agent-<id>`);
the hooks report `<project>`.

An explicit task (`state working`) takes over the screen while it runs; during a task,
`PreToolUse` steps also appear in its activity list as one dim line ("Editing x3"), under
the agent's own `msg` milestones. Set the label with
`&agent=Name` (default "Claude").

## Asking from a shell

```bash
H=http://nebuladeck.local/api; ID=q$RANDOM
curl -4 -s -m 3 -H 'Content-Type: application/json' \
  -d "{\"id\":\"$ID\",\"prompt\":\"Ship it?\",\"options\":[\"Yes\",\"No\"],\"timeout\":120}" $H/ask
for _ in $(seq 125); do a=$(curl -4 -s -m 3 "$H/answer?id=$ID"); [ "$a" = pending ] || break; sleep 1; done
echo "$a"   # answered:Yes | cancelled | empty (no screen)
```
