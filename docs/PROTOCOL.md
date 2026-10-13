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
| `ask` | `id`, `prompt`, `options[]` | Show buttons; device answers with `tap` |
| `ui` | `layout` | Replace the UI layout (declarative widget tree) |
| `clear` | | Clear messages |

## Device → host

| `t` | Fields |
|---|---|
| `hello` | `fw`, `w`, `h` |
| `pong` | |
| `tap` | `id`, `choice` (answer to an `ask`) |
| `touch` | `x`, `y`, `kind` (raw event, opt-in) |

## MCP tools (host)

`screen_set_state`, `screen_push_message`, `screen_ask` (returns the tap),
`screen_set_layout`, `screen_clear`, `screen_status`.
