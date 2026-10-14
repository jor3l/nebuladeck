#pragma once
// CI sets both: release builds use the version and "stable" (updates from the latest
// GitHub release); main builds use "dev-<run>-<sha>" and "dev" (updates from the rolling
// `dev` prerelease). Local builds never check for updates.
#ifndef FW_VERSION
#define FW_VERSION "local"
#endif
#ifndef FW_CHANNEL
#define FW_CHANNEL "local"
#endif
