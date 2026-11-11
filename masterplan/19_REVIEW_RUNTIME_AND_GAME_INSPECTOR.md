# 19 — Review Runtime and Game Inspector

## Purpose
Poko Mod must inspect and execute games in a controlled environment. Never run untrusted game code directly on a moderator's workstation with unrestricted access.

## Review Runtime
- Disposable environment per review session.
- Separate test accounts and identities.
- Test database and fake inventory.
- Simulated purchases and virtual currency.
- No production credentials.
- Resource limits for CPU, memory, disk and processes.
- Network egress denied by default; controlled test endpoints only.
- Session timeout and automatic cleanup.
- Snapshot of package hashes and runtime version.
- Moderator-visible logs and diagnostics.

## Test modes
- Normal play.
- Multi-player simulation.
- Offline test.
- Player-hosted mode test where feasible.
- Dedicated-server test.
- Network latency/loss simulation.
- Low-resource profile.
- Restart/reconnect test.
- Stress test with bounded limits.

## Inspector panels
- Game overview.
- Hierarchy/explorer.
- Asset inspector.
- Character/rig inspector.
- Animation timeline and event inspector.
- MUTE script viewer and static analysis.
- Service and permission inspector.
- Network/remote event inspector.
- Physics inspector.
- UI inspector.
- Audio inspector.
- Data/storage inspector.
- Performance profiler.
- Security findings.
- Test logs and crash reports.

## Evidence capture
Allow screenshots, short recordings and diagnostic bundles where justified. Redact account tokens, private player data and secrets. Set retention limits. Do not make sensitive evidence downloadable by default.

## Performance metrics
Collect frame time, memory, CPU, entity counts, script execution time, network throughput, server tick delay, error rates and crash data. Compare against documented test profiles; do not treat one machine's result as universal.

## Moderator workflow
The moderator should be able to jump from a finding to the exact script, asset or scene location. Provide internal notes separately from developer-visible findings.
