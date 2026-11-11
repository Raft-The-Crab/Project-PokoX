# 10 — Developer Portal and Analytics

## Developer home
Show:
- Projects and team memberships.
- Current published releases.
- Pending submissions and remaining review attempts.
- Server status and connection health.
- Reports requiring action.
- Recent platform announcements.
- Documentation shortcuts.

## Per-game dashboard
Potential metrics:
- Visits and unique players.
- Active players over time.
- Session duration distribution.
- Retention cohorts where privacy-compliant.
- Crash rate and error rate.
- Client performance distribution.
- Server uptime and disconnect rate.
- Supported platform breakdown.
- Favorites and follows.
- Revenue/virtual reward ledger where applicable.

## Analytics privacy
- Aggregate data by default.
- Avoid exposing individual player behavior unless necessary for safety or support.
- Set minimum cohort sizes to reduce re-identification.
- Explain collected metrics.
- Offer privacy controls where required.
- Set retention and deletion policies.

## Developer diagnostics
- Version health.
- Runtime compatibility.
- Package validation results.
- MUTE errors and stack traces.
- Service permission use.
- Server heartbeat history.
- Network quality indicators.
- Crash dump upload with sensitive data filtering.

## Team access
Analytics permissions should be separately configurable. A tester may need crash logs but not revenue data; an artist may need asset usage but not account administration.

## Small features
- CSV export for permitted aggregate metrics.
- Date range selector.
- Compare two releases.
- Annotate a graph with release dates.
- Alert when crash rate spikes.
- Alert when a server stops heartbeating.
- Explain metric definitions to prevent misleading interpretation.
