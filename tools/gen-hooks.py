#!/usr/bin/env python3
"""Writes skill/hooks.json: Claude Code hooks that report events to the NebulaDeck.

Each hook is one background curl (never blocks Claude) that sends only names, never content:
the event, the project (the session's root folder; a subagent's worktree
".../<project>/.claude/worktrees/agent-<id>" counts as <project>), and inside a subagent its
id and type (agent_id / agent_type, which Claude Code adds to events fired inside subagents),
plus the tool name (PreToolUse) and idle vs. needs-you (Notification).
Fields are read with grep from Claude Code's top-level keys, which come before tool_input.
"""
import json, os

URL = '"http://${NEBULADECK_HOST:-nebuladeck.local}/api/hook"'
EVENTS = ["SessionStart", "UserPromptSubmit", "PreToolUse", "Notification", "SubagentStart",
          "SubagentStop", "Stop", "SessionEnd"]


def command(ev):
    c = ("j=$(cat); "
         "f() { printf '%s' \"$j\" | grep -o \"\\\"$1\\\" *: *\\\"[^\\\"]*\\\"\" | head -n 1 | sed 's/^[^:]*: *\"//; s/\"$//'; }; "
         "p=${CLAUDE_PROJECT_DIR:-$(f cwd)}; p=${p%%/.claude/worktrees/*}; ")
    extra = ""
    if ev == "PreToolUse":
        extra = ' --data-urlencode "tool=$(f tool_name)"'
    if ev == "Notification":
        c += 'case "$(f message)" in *"waiting for your input"*) k=idle;; *) k=attn;; esac; '
        extra = ' --data-urlencode "kind=$k"'
    c += ('curl -4 -s -m 2 -G -X POST --data-urlencode "event=%s" --data-urlencode "project=${p##*/}" '
          '--data-urlencode "sub=$(f agent_id)" --data-urlencode "role=$(f agent_type)"%s %s >/dev/null 2>&1 &'
          % (ev, extra, URL))
    return c


hooks = {ev: [{"hooks": [{"type": "command", "command": command(ev)}]}] for ev in EVENTS}
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "skill", "hooks.json")
with open(out, "w") as f:
    f.write(json.dumps({"hooks": hooks}, indent=2) + "\n")
print("wrote", os.path.normpath(out))
