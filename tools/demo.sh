#!/usr/bin/env bash
# Walks the screen through every state with curl, the way an agent would.
# Device: nebuladeck.local, or NEBULADECK_HOST=<ip>.
set -u
URL="http://${NEBULADECK_HOST:-nebuladeck.local}/api"
nd() { b='{}'; [ $# -gt 1 ] && b=$2; curl -s -m 3 -H 'Content-Type: application/json' -d "$b" "$URL/$1" >/dev/null || echo "no device at $URL"; }

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
  a=$(curl -s -m 3 "$URL/answer?id=demo"); [ "$a" = pending ] || break; sleep 1
done
echo "answer: ${a:-<none>}"

nd state '{"status":"error","title":"Build failed: missing symbol in ui.cpp"}'
nd msg '{"text":"Build failed","level":"error"}'; sleep 4
nd state '{"status":"working","title":"Fixing the build","progress":0.9}'; sleep 3

echo "done: completed screen (tap the nebula for ripples, the logo for device info)"
nd done '{"summary":"Refactored the physics step. 214 tests passing.","tokens_in":48210,"tokens_out":9320,"duration":252}'
sleep 15

echo "bye: back to the nebula"
nd bye
