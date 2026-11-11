# 17 — Server Registry and Connection Services

Registry: server/game/developer IDs, package release, runtime version, mode, region if known, capacity, players, heartbeat and status.

Server gets unique identity and revocable credentials; private credentials stay on host. Registration validates package/runtime. Missed heartbeats move status to suspect then offline.

Connection: player requests session; backend validates game/server; issues short-lived single-use ticket; server validates ticket.

Exit gate: test server registers, heartbeats, accepts ticketed connection and can be revoked via CLI/API tests.
