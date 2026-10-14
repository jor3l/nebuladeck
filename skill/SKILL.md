---
name: nebuladeck
description: Show task status on the user's NebulaDeck desk screen (a WiFi touch display on their local network, driven with curl). Use at the start of any multi-step task, when you reach milestones, whenever you ask the user a question that has clear options (in or out of a task), and when you finish. Skip it for one-line answers.
---

# NebulaDeck screen

The user keeps a small touch screen on their desk so they can look away while you work. It sits
on their local network and takes JSON over HTTP, so plain `curl` drives it. Nothing else needs to be
installed.

Base URL: `http://nebuladeck.local/api` (if `$NEBULADECK_HOST` is set, use `http://$NEBULADECK_HOST/api`).
Always pass `-4 -m 3` and ignore failures: if the screen is off or unreachable, just carry on.
(`-4` matters: the device has no IPv6 address, and without it name lookups can stall for 5 s.)

```bash
nd() { b='{}'; [ $# -gt 1 ] && b=$2; curl -4 -s -m 3 -H 'Content-Type: application/json' -d "$b" "http://${NEBULADECK_HOST:-nebuladeck.local}/api/$1" >/dev/null; true; }
```

Each Bash call is a fresh shell, so define `nd` in the same command that uses it (or write the curl out).

Plain chat is covered separately: if the user installed the NebulaDeck hooks, the screen already
shows "Claude connected" with "Thinking..." / "Ready" on its own. Don't repeat that. The calls below
are for real tasks.

## When to use it

1. **Start of a task** (more than a couple of steps):
   `nd state '{"status":"working","title":"<what you are doing>","project":"<folder name>"}'`
2. **Progress**: send `state` again with a new title or `"progress":0.4` when the phase changes, not on every file.
3. **Milestones / problems**: `nd msg '{"text":"<one line>","level":"info|warn|error"}'`
4. **Any question for the user with clear options**: a decision, an approval, a yes/no, "A or B?".
   This applies at any time, not only during a task: if you are about to ask the user something in the
   chat that they could answer with a tap, ask it on the screen first. They may be away from the
   keyboard, and the hooks only show a generic "Needs you", never the question itself. Open-ended
   questions ("what should the title be?") stay in the chat only.
   Post an ask with your own short id, then poll for the answer:
   ```bash
   H="http://${NEBULADECK_HOST:-nebuladeck.local}/api"; ID=q$RANDOM
   curl -4 -s -m 3 -H 'Content-Type: application/json' -d "{\"id\":\"$ID\",\"prompt\":\"Delete 12 files?\",\"options\":[\"Delete\",\"Skip\"],\"timeout\":120}" "$H/ask" >/dev/null || echo unreachable
   for _ in $(seq 125); do a=$(curl -4 -s -m 3 "$H/answer?id=$ID"); [ "$a" = pending ] || break; sleep 1; done; echo "$a"
   ```
   - Prints `answered:<option>`, or `cancelled` (nobody answered in time). Empty or `unreachable` = no screen.
   - If it isn't `answered:...`, ask in the chat instead (the screen ask has already expired). Always set
     `timeout` so you never hang; ~120 s gives someone across the room time to look.
   - Max 4 options, keep each under ~16 characters. An ask already switches the screen to "needs you".
5. **Finished**: `nd done '{"summary":"<one-sentence result>"}'`
   - Add `"tokens_in"`, `"tokens_out"` and `"duration"` (seconds) only if you actually know them; the
     token card is hidden otherwise.
   - Always send this as the last thing you do, even when the task failed (say so in the summary,
     or send `state` with `"status":"error"` if you are stuck and waiting).

## Reference

| Call | Body |
|---|---|
| `state` | `status` (`working\|waiting\|done\|error\|idle`), `title`, `project`, `progress?` 0-1 |
| `msg` | `text`, `level?` (`info\|warn\|error`), `ttl?` seconds |
| `ask` | `id?`, `prompt`, `options` (1-4), `timeout?` seconds → `GET answer?id=` |
| `done` | `summary`, `tokens_in?`, `tokens_out?`, `duration?` |
| `clear`, `bye` | none (`bye` sends the screen back to its idle nebula) |

`GET /api/info` tells you whether a screen is there. Full API: docs/PROTOCOL.md in
github.com/jor3l/nebuladeck.

## Installing the hooks (only when the user asks)

`hooks.json` next to this file holds Claude Code hooks that keep the screen alive during plain chats.
To install them, merge its `hooks` entries into `~/.claude/settings.json` (keep existing hooks; back the
file up first), then tell the user to restart Claude Code. Each hook is a background `curl`, so it never
slows the session down.

## Style

- Titles and messages are read from across a desk: short, plain, no markdown, under ~60 characters
  for titles and ~90 for messages. Use the current folder name as the project.
- Escape quotes properly inside the JSON. Don't put secrets (keys, passwords) on the screen.
- Don't spam. A handful of updates per task is plenty.
