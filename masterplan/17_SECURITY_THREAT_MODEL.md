# 17 — Security Threat Model

## Assets to protect
- Player accounts and private data.
- Moderator accounts and sensitive evidence.
- Developer projects and unpublished packages.
- Production services and signing keys.
- Economy ledger and creator revenue records.
- Game server credentials.
- Package integrity and update channels.

## Threat actors
- Malicious players.
- Malicious or compromised developers.
- Cheaters and bot operators.
- Account takeover attackers.
- Malicious packages and dependencies.
- Compromised moderator accounts.
- Abuse of free hosting or platform quotas.

## Core controls
### Client
- No privileged secrets embedded in binaries.
- Secure token storage.
- Signed updates.
- Package signature/hash verification.
- Safe archive extraction.
- Least OS permissions.
- No arbitrary native code from games.

### MUTE/game runtime
- Capability-based permissions.
- Instruction, memory, entity and network quotas.
- No unrestricted filesystem or process access.
- External network denied by default or allowlisted.
- Isolate games from each other and host environment.

### Server
- Unique server identity and revocable credentials.
- Short-lived, single-use player connection tickets.
- Server-side validation of important game state.
- Rate limits, packet limits and replay protection.
- Restricted OS account and resource limits.
- Never trust client-reported currency, inventory or rewards.

### Backend
- Least-privilege service credentials.
- Secret rotation.
- Request validation and rate limits.
- Audit logs for privileged actions.
- Database backups and restore tests.
- Dependency and supply-chain scanning.

### Moderation
- Isolated Review Runtime.
- Separate test data and accounts.
- Restricted evidence access.
- MFA and short-lived moderator sessions.
- Immutable audit trail.
- Two-person approval for exceptionally sensitive actions where feasible.

## Security testing
Build a Poko Security Lab game/package that tests:
- Infinite loops.
- Memory exhaustion.
- Entity spam.
- Malformed package archives.
- Path traversal.
- Invalid/replayed tickets.
- Unauthorized service calls.
- Network flooding.
- Data access boundary failures.
- Reward manipulation.
- Crash and recovery behavior.

## Limits
No client anti-cheat can guarantee cheat-proof execution on a device controlled by the player. Protect valuable state on authoritative servers and detect abuse rather than promising perfect prevention.
