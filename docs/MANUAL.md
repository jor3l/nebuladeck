# NebulaDeck user manual

How to use the screen day to day, once it's built and flashed. To get there, see
[Build your NebulaDeck](../README.md#build-your-nebuladeck) and the [setup page](https://jor3l.github.io/nebuladeck/).

## First start

1. Plug the USB-C cable into your computer or any USB charger. USB only powers the screen;
   everything else goes over WiFi.
2. A new screen says **Welcome** and shows a QR code: **Scan to connect me to WiFi**.
3. Scan it with your phone, or open [jor3l.github.io/nebuladeck](https://jor3l.github.io/nebuladeck/)
   and choose **Connect to WiFi**. The page talks to the screen over Bluetooth, lists the networks
   it can see and sends it the password you type. Use Chrome or Edge on Android or a computer; on
   iPhone or iPad use the Bluefy browser.
4. The screen shows **Connecting to WiFi**, then **Agent not connected**: it's online and
   waiting for an agent. Bluetooth switches off.
5. On the setup page, choose **Set up your agent** and follow the steps for yours (Claude Code,
   Codex CLI, Gemini CLI, Cursor, GitHub Copilot or any other agent that can run `curl`).

The screen remembers its WiFi network, so after a power cut it reconnects by itself.

## What the screen shows

### Nobody connected

A slow, drifting nebula with **Agent not connected**. Touch it and it ripples under your finger;
that's all it does until an agent starts talking to it.

### An agent is working

When an agent starts a task, the screen shows:

- its name and project at the top, and a coloured status: **WORKING**, **NEEDS YOU**, **DONE** or **ERROR**;
- the task title in large type, with a progress bar if the agent reports one;
- **ACTIVITY**: its latest steps and milestones, newest first. Repeated steps are counted
  ("Editing ×3"). Warnings show in amber and errors in red.

With the optional Claude Code hooks, plain chats show up too: **Thinking...**,
**Running a command...**, **Needs you** and **Ready**.

### A question

When an agent needs a decision, the question appears with up to four buttons. **Tap one** and
the agent carries on with your choice. If you'd rather answer in the chat, just do that: the
question goes away when it times out or when the agent moves on. The screen stays lit for as long
as a question is waiting.

### Task finished

At the end of a task you see the agent's one-line summary. If it reported its token use, a
**TOKENS** card shows **IN** and **OUT** totals and how long the task took. The summary stays
until the agent starts something new.

### Several agents at once

When two or more agents (or one agent in several projects) are using the screen, it shows a
card for each one, most urgent first: questions and agents that need you, then busy ones, then
the rest. Each card has a label:

| Label | Meaning |
|---|---|
| **QUESTION** | Waiting for you to tap an answer |
| **NEEDS YOU** | The agent is waiting for you in the chat |
| **WORKING** / **ACTIVE** | Busy on a task / busy in a chat |
| **DONE** | Finished; shows its summary |
| **ERROR** | The task failed |
| **READY** | Connected, nothing to do |
| **RECONNECTING** | Saved from before a restart; it comes back when the agent next checks in |

- **Drag** the list up and down to scroll.
- **Tap a card** to open that agent full screen. **‹ All agents** at the top right takes you back.
- **Swipe a card sideways** to dismiss it (a question on it is cancelled, so the agent asks in the chat).
  It comes back if the agent sends something new.
- Subagents (helpers an agent starts, like Claude Code's Explore) show as small rows inside their
  session's card and disappear when they finish.

An agent that goes quiet for 30 minutes, or says goodbye, drops off the screen.

## The logo menu

Tap the **NEBULADECK** logo at the top to open the device menu. It shows the firmware **Version**,
the update **Channel**, the WiFi **Network** and the **Address** (`nebuladeck.local` and the IP),
plus three buttons:

- **Check updates**: looks for a new release now. If one exists, the button becomes **Install update**.
- **Forget WiFi**: tap it, then tap **Tap to confirm** within 4 seconds. The screen goes back to
  the Welcome QR code so you can join another network.
- **Turn screen off**: the screen goes dark until you tap it. Agents keep updating it meanwhile,
  so you see the latest state when you wake it.

Swipe the menu down or tap outside it to close it.

## Changing WiFi network

Either use **Forget WiFi** in the logo menu, or **press and hold** anywhere on the nebula for
5 seconds (**Keep holding to set up WiFi** shows the progress). The Welcome QR code comes back; set
up the new network as on the first start. If you change your mind, tap **Cancel** at the bottom
to go back to the network it had.

## Screen sleep

After 3 minutes with no touch and no agent activity, the backlight fades off. A tap or the next
agent update wakes it; the waking tap doesn't press anything. The screen stays on while a question
is waiting, and for up to 10 minutes while an agent needs you.

## Updates

Screens flashed from the setup page check GitHub for new firmware every 12 hours. When there's
one, a sheet shows **Update available** with **Update** and **Skip**:

- **Update** installs it over WiFi and restarts. Agents on the screen survive the restart.
- **Skip** ignores this version until a newer one comes out.
- Swiping the sheet down means "later": it comes back with the next check.

Screens flashed with **Dev** follow the newest build of `main`; screens flashed with **Latest**
get stable releases only. A firmware you build yourself (`make flash`) never checks
(**Off (local build)** in the menu).

## Troubleshooting

**The agent doesn't show up.** From a terminal on the same computer run
`curl -4 http://nebuladeck.local/api/info`. If that fails:

- `.local` names don't work on some networks. Open the logo menu, note the IP, and use it instead
  (the setup page's **Set up your agent** rewrites every command for you).
- Keep the `-4`: some systems try IPv6 first and time out.
- Check the computer and the screen are on the same network (guest networks often isolate devices).
- Codex CLI runs commands with networking off by default; see the Codex tab on the setup page.

**The screen is dark.** It's asleep or turned off: tap it.

**Stuck on Connecting to WiFi.** The password may be wrong or the network out of range. Hold the
screen for 5 seconds to set it up again. The screen only joins 2.4 GHz networks.

**Start over.** Re-flash the firmware from the setup page (**Flash device**). If you choose to
erase the device during the install, it also forgets its WiFi.

For the full HTTP API, see [PROTOCOL.md](PROTOCOL.md).
