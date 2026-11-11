# 14 — Backend and Database Foundation

Backend follows stable runtime contracts and remains independent of rendering/UI.

Domains: identity, accounts, catalog, package delivery, server registry, publishing, moderation, groups/teams, economy ledger, marketplace, notifications, analytics and support.

Current candidates: Cloudflare Workers/TypeScript, D1, R2, PostgreSQL, Valkey, Firebase Authentication and Go services. Hugging Face free CPU is for experiments/non-critical workloads until reliability is proven. Free-tier fallback requires durable queues, replicated state, health checks and tested failover.

Use migrations, transactions, idempotency, UTC timestamps, append-only ledgers and clear source-of-truth ownership.

Exit gate: APIs work through CLI/integration tests with auth, rate limits, logging, migrations and backups.
