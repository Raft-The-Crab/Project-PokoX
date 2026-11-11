# 16 — API and Data Architecture

## Service design
Prefer a small number of clearly owned services over many tiny services. Define which service owns each record and which service is allowed to mutate it.

Suggested domains:
- Identity/account.
- Player/social.
- Groups/teams.
- Game catalog.
- Publishing/review.
- Package delivery.
- Server registry.
- Economy ledger.
- Marketplace.
- Reports/moderation.
- Notifications.
- Analytics/support.

## API conventions
- Version APIs.
- Use stable resource IDs.
- Validate all input server-side.
- Apply authorization on every request.
- Use pagination and bounded query sizes.
- Return consistent error codes.
- Use idempotency keys for payment-like, reward and publishing operations.
- Rate-limit by account, IP/device signals and endpoint risk.
- Include request/correlation IDs.
- Never trust client-provided prices, balances, permissions or review status.

## Data rules
- Store timestamps in UTC; render local time in clients.
- Store money/points as integer minor units, not floating point.
- Use explicit enums with migration strategy.
- Keep immutable audit/ledger records.
- Use transactions for state transitions.
- Avoid cross-database transactions where possible; use outbox/saga patterns.
- Encrypt sensitive data at rest and in transit.

## Package metadata
A game manifest should declare package identity, version, engine compatibility, supported platforms, content labels, networking modes, service permissions, dependencies, entry points and hashes. The server validates it independently.

## Data placement
- D1: small metadata/workflow tables if its limits and transactional behavior fit.
- PostgreSQL: relational records and complex queries.
- R2: immutable packages, images, logs and larger artifacts.
- Valkey: ephemeral cache/rate-limit state, not the only copy of durable data.

Avoid duplicating authoritative balances, permissions or review counts across caches.
