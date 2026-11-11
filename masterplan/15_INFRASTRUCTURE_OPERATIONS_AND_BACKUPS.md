# 15 — Infrastructure, Operations and Backups

## Service boundaries
Potential initial responsibilities:
- Cloudflare Workers: stateless API routing, validation, lightweight orchestration and edge logic within quota.
- D1: small relational metadata and workflow records where suitable.
- R2: package blobs, thumbnails and generated artifacts.
- PostgreSQL: relational data needing stronger transactional/query features.
- Valkey/Redis: caching, rate limiting and ephemeral coordination.
- Firebase Authentication: identity provider, with backend token validation.
- Go services: selected heavier or long-running backend tasks where reliable hosting is available.
- Hugging Face free CPU: experiments, prototypes or non-critical services only until reliability is proven.

Do not assume a free-tier service can be a production fallback. A fallback must have health checks, replicated state, queue semantics, retry policies and a tested failover plan.

## Capacity
Measure actual request volume, storage growth, database usage, egress, CPU and memory. Establish quotas and alert thresholds before launch. Free quotas can change.

## Observability
- Structured logs with correlation IDs.
- Metrics for latency, error rate and saturation.
- Health checks and synthetic tests.
- Service status dashboard.
- Alerts for failed jobs, database limits and storage growth.
- Redaction of tokens, personal data and sensitive moderation evidence.

## Backups
- Automated database backups.
- R2 object versioning or equivalent protection where available.
- Encrypted backup copies in a separate account/provider where feasible.
- Documented retention schedule.
- Restore tests, not just backup-success alerts.
- Recovery point objective (RPO) and recovery time objective (RTO) for each service.

## Disaster recovery
Document:
- How to restore identity-linked metadata.
- How to restore game package indexes and blobs.
- How to rotate compromised credentials.
- How to restore moderation records.
- How to rebuild server registrations.
- How to communicate outages.
- How to operate in read-only/degraded mode.

## Developer-hosted game servers
Poko can provide runtime, registration, discovery, diagnostics and documentation, but the developer is responsible for host hardware, internet connection, firewall, uptime and local backups. Poko should make this responsibility clear in Studio.
