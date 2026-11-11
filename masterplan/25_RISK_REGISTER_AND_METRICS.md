# 25 — Risk Register and Success Metrics

## Major risks

| Risk | Impact | Mitigation |
|---|---|---|
| Free hosting instability | Platform outage | Keep critical services on reliable hosting before public launch; degrade gracefully |
| Scope too large | Project never ships | Small Alpha scope and milestone gates |
| Review queue overload | Updates delayed | Submission limits, reviewer capacity planning and clear SLAs |
| Malicious game code | Host/platform compromise | Isolated runtime, OS restrictions and resource limits |
| Reward farming | Economy inflation | Eligibility, caps, anomaly detection and budget limits |
| Child-safety incident | Severe user harm | Restricted safety workflow, trained staff and legal escalation |
| Package tampering | Malware or exploit | Signatures, hashes and safe extraction |
| Data loss | Account/game loss | Backups and restore drills |
| Dependency license issue | Legal/release risk | Software bill of materials and license review |
| Developer-host outage | Game unavailable | Clear developer responsibility and server health status |

## Product metrics
- Successful game launches.
- Crash-free sessions.
- Download completion rate.
- Time to first playable experience.
- Creator project completion.
- Submission-to-decision time.
- Review rework rate.
- Appeal overturn rate.
- Report response time.
- Server heartbeat availability.
- Restore test success.
- Accessibility defect count.

## Guardrails
Do not optimize only for daily active users or playtime. Pair engagement metrics with safety reports, user satisfaction, crash rate, moderation burden and abuse indicators.

## Operational objectives
Define service-specific SLOs only after baseline measurements. Avoid inventing uptime guarantees before hosting and staffing are proven.
