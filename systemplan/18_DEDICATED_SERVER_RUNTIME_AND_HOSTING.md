# 18 — Dedicated Server Runtime and Hosting

Developers provide hosting for their games. Poko supplies shared headless runtime, `.pkxx`, registration/discovery, diagnostics and documentation. Poko initially hosts official games.

Headless means no graphics/audio/player UI; server still runs authoritative game logic, relevant physics, validation, sessions, networking and persistence. Capacity must be benchmarked per game.

Runtime loads/verifies `.pkxx`, checks compatibility, starts MUTE, registers, accepts ticketed connections, simulates, persists permitted state and sends heartbeat.

CLI: `poko-server doctor`, `status`, `logs`, `restart`, `update`, `verify-package`.

Exit gate: external test developer hosts, registers, connects players, updates package and recovers from crash.
