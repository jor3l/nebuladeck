#!/usr/bin/env bash
# Walks the screen through every state using the CLI, the way an agent would.
# The daemon owns the serial port, so this goes through `nebuladeck`, not the port.
set -u
nd() { node "$(dirname "$0")/../host/bin/nebuladeck.js" "$@"; }

echo "presence: chatting"
nd presence "Thinking..." --new-turn; sleep 3
nd presence "Reading..."; sleep 2
nd presence "Needs you" --attn; sleep 2
nd presence "Ready" --end-turn; sleep 2

echo "task: working"
nd state working "Refactoring the physics step" --project flightsim --progress 0.2; sleep 3
nd msg "Ran 214 tests, all passing"; sleep 2
nd msg "Lint warnings in 3 files" --level warn
nd state working "Refactoring the physics step" --progress 0.65; sleep 3

echo "ask: tap an option on the screen (30 s)"
answer=$(nd ask "Delete 12 unused files?" Delete "Show list" Skip --timeout 30)
echo "answer: ${answer:-<none>}"

nd state error "Build failed: missing symbol in ui.cpp"
nd msg "Build failed" --level error; sleep 4
nd state working "Fixing the build" --progress 0.9; sleep 3

echo "done: completed screen (tap the nebula for ripples)"
nd done "Refactored the physics step. 214 tests passing." --tokens-in 48210 --tokens-out 9320 --duration 252
sleep 15

echo "bye: back to the nebula"
nd bye
