# nebuladeck (host)

Daemon + CLI for the NebulaDeck screen. See the [project README](../README.md) and
[setup guide](../docs/SETUP-MAC.md).

```bash
npm install && npm link      # or: make install  (from the repo root)
nebuladeck state working "Hello"
```

`src/daemon.js` owns the serial port and heartbeat; the CLI talks to it over
`~/.nebuladeck/daemon.sock`. Logs: `~/.nebuladeck/daemon.log`.
