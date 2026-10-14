# Protocol (draft)

Transport: USB CDC serial, 115200+ (native USB, baud ignored), UTF-8,
one JSON object per line (`\n`). Host and device both ignore unknown fields.

## Host → device

| `t` | Fields | Effect |
|---|---|---|
| `hello` | `v` | Handshake; device replies `hello` with firmware version, screen size |
| `ping` | | Heartbeat, every 1 s. Missing for 3 s → device returns to nebula idle |
| `state` | `project`, `status` (`idle\|working\|waiting\|done\|error`), `title`, `progress?` (0-1) | Set the main status |
| `msg` | `text`, `level?` (`info\|warn\|error`), `ttl?` | Push a message line |
| `done` | `summary`, `tokens_in`, `tokens_out`, `duration?` (s) | Task finished: nebula screen with the completion message, "Agent idle" and a floating token card. Stays until the next `state`/`ask`/`bye` |
| `ask` | `id`, `prompt`, `options[]` | Show buttons; device answers with `tap` |
| `ui` | `layout` | Replace the UI layout (declarative widget tree) |
| `clear` | | Clear messages |
| `ask_cancel` | | Remove an open `ask` (host gave up waiting) |
| `bye` | | Clean disconnect: back to the nebula immediately (no 3 s wait) |

## Device → host

| `t` | Fields |
|---|---|
| `hello` | `fw`, `w`, `h` |
| `pong` | |
| `tap` | `id`, `choice` (answer to an `ask`) |
| `touch` | `x`, `y`, `kind` (raw event, opt-in) |

## CLI (host)

Agents don't speak this protocol directly. The `nebuladeck` CLI talks to a local daemon that owns
the serial port and the heartbeat:

| Command | Sends |
|---|---|
| `nebuladeck state <status> "<title>" [--progress]` | `state` |
| `nebuladeck msg "<text>" [--level] [--ttl]` | `msg` |
| `nebuladeck ask "<prompt>" <opt>...` | `ask`, prints the `tap` answer |
| `nebuladeck done "<summary>"` | `done` (token totals from the session transcript) |
| `nebuladeck clear` / `bye` | `clear` / `bye` |
