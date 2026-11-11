# 26 — Testing, Security and Release Gates

Test levels: unit, integration, contract, end-to-end, fuzz, performance/load, manual usability and accessibility.

Engine: math, resource lifetime, renderer fallback, asset parser fuzzing, physics regression, serialization corruption.

Runtime: MUTE sandbox escape attempts, infinite loops, memory exhaustion, entity spam and permission violations.

Network/server: malformed packets, replay, spoofing, reconnect, ticket expiry, revocation and resource exhaustion.

Backend: auth boundaries, migrations, idempotency, rate limits, backup restoration and secret rotation.

Moderation: isolation, no production secret access, attempt accounting, appeal scope, evidence access and audit.

No public release with critical security defects, untested recovery, exposed secrets, unverified package signatures or broken safety escalation.
