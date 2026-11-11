# 35 — Creator Workflow, Playtesting and Project Portability

## Goal

Make Poko Studio easier to learn, more transparent to debug and safer to publish, while preserving creators' ability to back up and understand their projects.

## Guided-to-advanced workflow

### New creator mode
- Choose a starter template: Obby, block-building, exploration, small multiplayer arena or blank project.
- Guided project setup and asset import.
- Explain scene, entity, component, script and package concepts.
- MUTE lessons with runnable examples.
- Clear errors with suggested fixes.
- One-click local playtest.
- Publishing checklist with privacy, content and network disclosures.

### Advanced creator mode
- Full scene hierarchy and inspector.
- MUTE debugger, profiler and event trace.
- Network message inspector.
- Asset dependency graph.
- Package manifest and build logs.
- Runtime version selection where supported.
- Build reproducibility and signing information.
- Version control integration and branch-aware workflows.

## Creator Playtest Rooms

- Create a private test session from a specific build hash.
- Invite selected testers with expiring access.
- Provide feedback categories: gameplay, bug, performance, accessibility and general.
- Allow testers to attach logs and screenshots only with clear consent and privacy filtering.
- Associate feedback with game version, runtime version and device class.
- Keep test data separate from production player data.
- Let developers close a playtest and export a summary.
- Prevent playtest access from granting project edit or publish rights.

## Project portability

### `.pk` project format
- Versioned manifest.
- Scene and resource references.
- MUTE source and dependency declarations.
- Asset license/attribution metadata.
- Project settings and input maps.
- Build configuration.
- Optional local test fixtures.
- Integrity hashes.

### Export and backup
- Export a complete project archive.
- Generate a dependency report.
- Warn about assets that cannot legally or technically be exported.
- Provide local backups and optional versioned remote backups.
- Support project migration tools between schema versions.
- Preserve project history and restore points.

Do not promise full compatibility with third-party engines or formats. Import/export support must be explicit and tested.

## Creator reliability and release controls

- Release channels: draft, private test, beta/limited test if approved, and public.
- Build hash and manifest shown for each release.
- Roll back to a previous approved release where policy and safety allow.
- Keep previous approved release live during ordinary review.
- Record runtime compatibility.
- Provide a release checklist and changelog.
- Use staged rollout only if infrastructure supports it.
- Never bypass required human review for a normal game update.

## Creator analytics

Show aggregated, privacy-conscious metrics:
- Game launches and unique players where properly defined.
- Session length distributions.
- Crash and disconnect rates.
- Device/runtime breakdown.
- Tutorial completion and funnel metrics.
- Achievement unlock percentages.
- Server uptime/heartbeat evidence for developer-hosted servers.
- Feedback themes and bug reports.

Use minimum cohort thresholds and avoid exposing individual player behavior unnecessarily. Explain metrics definitions and sampling.

## Acceptance criteria

- A beginner can create and run a starter project without reading the entire manual.
- A developer can reproduce a build from a recorded manifest and toolchain version within documented limits.
- A project export includes dependency and license information.
- Playtest feedback is tied to an immutable build identifier.
- Analytics do not expose individual player histories by default.
