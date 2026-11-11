# 23 — Alpha, Beta and Release Roadmap

## Stage 0 — Foundations
Deliver:
- Repository and build conventions.
- CMake/build pipeline.
- CI compilation and basic tests.
- Logging and crash reporting.
- Package manifest draft.
- Core API and security design.
- License inventory.

Exit gate: reproducible builds on the primary Windows development environment.

## Stage 1 — Engine/client prototype
- SDL window/input.
- Basic renderer.
- Asset loading.
- Camera and movement.
- R6 placeholder.
- Physics prototype.
- Basic UI shell.
- First Obby test scene.
- Basic block placement.

Exit gate: stable local test scene with measured frame time and memory.

## Stage 2 — Platform Alpha
- Windows Player and Studio shell.
- Authentication for online features.
- Game catalog and package delivery.
- Offline game launch.
- Basic MUTE interpreter and sandbox.
- First official game prototype.
- Basic developer publishing.
- Human review queue and four-attempt tracking.
- Package verification.
- Isolated review runtime before external submissions.
- Basic Poko Mod reports and audit log.

Exit gate: internal users can create, submit, review, approve and play one game safely.

## Stage 3 — Closed Beta
- Friends and basic groups.
- Developer teams and roles.
- Dedicated server runtime and `.pkxx`.
- Server registration and diagnostics.
- Content labels.
- Appeals.
- Game Inspector modules.
- Player reporting and emergency suspension.
- Android prototype on real devices.
- Backups and restore tests.

Exit gate: external creators can publish under controlled capacity; moderation queue is manageable.

## Stage 4 — Public launch
- Stable Windows Player and Studio.
- Tested Android support if ready.
- Two polished official games.
- Support/help center.
- Moderation staffing and escalation process.
- Privacy and legal review for launch markets.
- Operational status page.
- Incident response and recovery drills.

Exit gate: all critical security, safety, reliability and support criteria met.

## Stage 5 — Growth
- Marketplace.
- Advanced analytics.
- More character rigs.
- More MUTE libraries.
- Advanced discovery.
- Expanded social tools.
- Additional server hosting integrations.

## Delivery discipline
Estimate with prototypes, not guesses. Keep a risk buffer. Do not start every subsystem simultaneously. Each milestone needs a demo, tests, documented limitations and a rollback plan.
