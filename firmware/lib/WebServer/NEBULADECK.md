# WebServer (vendored)

Arduino-ESP32 2.0.17's WebServer library, copied here so this project's copy takes precedence.
Changes: `HTTP_MAX_DATA_WAIT` is 3000 ms instead of 5000 ms (see the comment in
`src/WebServer.h`), and a `silentClients` counter of connections that never sent a request. Everything else is upstream (LGPL-2.1, see the library's sources).
