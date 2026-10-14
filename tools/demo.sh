#!/usr/bin/env bash
# Walks the screen through every state with curl, the way an agent would.
# Device: nebuladeck.local, or NEBULADECK_HOST=<ip>.
set -u
URL="http://${NEBULADECK_HOST:-nebuladeck.local}/api"
# Requests without an agent are Claude's (the device would otherwise name it "Agent").
nd() { b='{}'; [ $# -gt 1 ] && b=$2
  case "$b" in *'"agent"'*) ;; '{}') b='{"agent":"Claude"}';; *) b="{\"agent\":\"Claude\",${b#\{}";; esac
  curl -4 -s -m 3 -H 'Content-Type: application/json' -d "$b" "$URL/$1" >/dev/null || echo "no device at $URL"; }

echo "presence: chatting"
nd presence '{"text":"Thinking...","new_turn":true}'; sleep 3
nd presence '{"text":"Reading..."}'; sleep 2
nd presence '{"text":"Needs you","attn":true}'; sleep 2
nd presence '{"text":"Ready","end_turn":true}'; sleep 2

echo "task: working"
nd state '{"status":"working","title":"Refactoring the physics step","project":"flightsim","progress":0.2}'; sleep 3
nd msg '{"text":"Ran 214 tests, all passing"}'; sleep 2
nd msg '{"text":"Lint warnings in 3 files","level":"warn"}'
nd state '{"status":"working","title":"Refactoring the physics step","project":"flightsim","progress":0.65}'; sleep 3

echo "ask: tap an option on the screen (30 s)"
nd ask '{"id":"demo","prompt":"Delete 12 unused files?","options":["Delete","Show list","Skip"],"timeout":30}'
for _ in $(seq 32); do
  a=$(curl -4 -s -m 3 "$URL/answer?id=demo"); case "$a" in answered:*|cancelled) break;; esac; sleep 1
done
echo "answer: ${a:-<none>}"

nd state '{"status":"error","title":"Build failed: missing symbol in ui.cpp"}'
nd msg '{"text":"Build failed","level":"error"}'; sleep 4
nd state '{"status":"working","title":"Fixing the build","progress":0.9}'; sleep 3

echo "done: completed screen (tap the nebula for ripples, the logo for device info)"
nd done '{"summary":"Refactored the physics step. 214 tests passing.","tokens_in":48210,"tokens_out":9320,"duration":252}'
sleep 15

# Claude Code hook events, as skill/hooks.json sends them: hook <event> <project> [sub role] [extra query]
hook() { q="event=$1&project=$2"; [ $# -gt 2 ] && [ -n "$3" ] && q="$q&sub=$3&role=$4"; [ $# -gt 4 ] && q="$q&$5"
  curl -4 -s -m 3 -X POST "$URL/hook?$q" >/dev/null || echo "no device at $URL"; }

echo "multiagent: flightsim delegates to two subagents"
hook UserPromptSubmit flightsim; sleep 1
hook PreToolUse flightsim "" "" tool=Agent; sleep 1
hook SubagentStart flightsim a1 Explore; sleep 1
hook SubagentStart flightsim a2 general-purpose; sleep 1
hook PreToolUse flightsim a1 Explore tool=Grep
hook PreToolUse flightsim a2 general-purpose tool=Edit; sleep 3

echo "sessions: more agents join (one card each)"
nd state '{"agent":"Codex","project":"webapp","status":"working","title":"Migrating to the new router","progress":0.4}'
nd msg '{"agent":"Codex","project":"webapp","text":"12 of 30 routes moved"}'; sleep 2
nd presence '{"agent":"Gemini","project":"docs","text":"Ready","new_turn":true}'; sleep 2
hook SessionStart nebuladeck; sleep 2
nd presence '{"agent":"Cursor","project":"api","text":"Ready","new_turn":true}'; sleep 3
hook Stop flightsim; sleep 3   # its turn ended, but its subagents keep it busy

echo "attention: cards that need you move to the top"
nd presence '{"agent":"Cursor","project":"api","text":"Needs you","attn":true}'; sleep 4
hook Notification nebuladeck "" "" kind=attn; sleep 4
echo "  ...and drop back once handled"
nd presence '{"agent":"Cursor","project":"api","text":"Thinking..."}'; sleep 3
hook PreToolUse nebuladeck "" "" tool=Bash; sleep 3

echo "attention: a subagent needs you (its session's whole card turns amber)"
hook Notification flightsim a1 Explore kind=attn; sleep 4
hook PreToolUse flightsim a1 Explore tool=Read; sleep 3

echo "attention: Codex asks (tap the card, then an option; 20 s)"
nd ask '{"agent":"Codex","project":"webapp","id":"demo2","prompt":"Keep the old routes as redirects?","options":["Keep","Drop"],"timeout":20}'
for _ in $(seq 22); do
  a=$(curl -4 -s -m 3 "$URL/answer?id=demo2"); case "$a" in answered:*|cancelled) break;; esac; sleep 1
done
echo "answer: ${a:-<none>}"
nd state '{"agent":"Codex","project":"webapp","status":"working","title":"Migrating to the new router","progress":0.8}'; sleep 3

echo "wrap-up: subagents finish, sessions settle"
hook SubagentStop flightsim a1 Explore; sleep 2
hook SubagentStop flightsim a2 general-purpose; sleep 2
hook Stop nebuladeck
nd presence '{"agent":"Cursor","project":"api","text":"Ready","end_turn":true}'
nd done '{"agent":"Codex","project":"webapp","summary":"Moved 30 routes to the new router.","tokens_in":31540,"tokens_out":6210,"duration":188}'; sleep 6

echo "bye: back to the nebula"
nd bye '{"agent":"Codex","project":"webapp"}'
nd bye '{"agent":"Gemini","project":"docs"}'
nd bye '{"agent":"Cursor","project":"api"}'
hook SessionEnd nebuladeck
hook SessionEnd flightsim
nd bye
