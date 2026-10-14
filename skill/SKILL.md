---
name: nebuladeck
description: Show task status on the user's NebulaDeck desk screen (a USB touch display). Use at the start of any multi-step task, when you reach milestones, when you need the user to choose something, and when you finish. Skip it for one-line answers.
---

# NebulaDeck screen

The user keeps a small USB touch screen on their desk so they can look away while you work.
Drive it with the `nebuladeck` command. It never blocks you: with no screen attached
the commands exit 0 and print a one-line notice, so just carry on.

## When to use it

1. **Start of a task** (more than a couple of steps): `nebuladeck state working "<what you're doing>"`
2. **Progress**: update the title or `--progress 0..1` when the phase changes, not on every file.
3. **Milestones / problems**: `nebuladeck msg "<one line>" [--level warn|error]`
4. **Need the user** (a decision, an approval): `nebuladeck ask "<question>" "<option1>" "<option2>"`
   - Prints the tapped option on stdout. Exit 2 = nobody answered within `--timeout`, exit 3 = no screen.
   - On exit 2 or 3, ask in the chat instead. Use `--timeout 120` or similar so you never hang.
   - Max 4 options, keep each under ~16 characters. Also set `nebuladeck state waiting "<why>"` is not needed, `ask` does it.
5. **Finished**: `nebuladeck done "<one-sentence result>"`
   - Token totals are filled in automatically from the session transcript.
   - Always send this as the last thing you do, even when the task failed (say so in the summary,
     or use `nebuladeck state error "<what broke>"` if you are stuck and waiting).

## Reference

```
nebuladeck state <working|waiting|done|error|idle> "<title>" [--project P] [--progress 0.4]
nebuladeck msg "<text>" [--level info|warn|error] [--ttl SECONDS]
nebuladeck ask "<prompt>" <opt> [opt ...] [--timeout SECONDS]
nebuladeck done "<summary>" [--no-tokens] [--tokens-in N --tokens-out N]
nebuladeck clear | bye | status
```

## Style

- Titles and messages are read from across a desk: short, plain, no markdown, under ~60 characters
  for titles and ~90 for messages. Project name defaults to the current folder.
- Don't spam. A handful of updates per task is plenty.
