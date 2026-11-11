# Core API and security design (Phase 0 draft)

**Status:** design draft, not an approved implementation contract.
**Scope:** Stage 0 deliverable "Core API and security design" (`masterplan/23`).
**Sources:** `systemplan/01`, `systemplan/03`, `systemplan/30`, `masterplan/17`,
`masterplan/16`.

## Purpose

Fix the module boundaries before subsystems are implemented so that engine core
never acquires dependencies on graphics, backend vendors or final UI. This is a
design record; concrete method signatures belong to the subsystem that owns
each interface.

## Early interface seams

Declared as boundary types in `engine/core/include/poko_api_seams.h`. Phase 0
declared them as `STUB` placeholders; Phase 1 gives the two engine-core-owned
interfaces real signatures and implementations. The rest remain `STUB`.

| Interface | Owning subsystem | Phase | Boundary rule |
|---|---|---|---|
| `IPlatformFileSystem` | engine core / platform abstraction | 1 (implemented) | root-scoped `LocalFileSystem`, traversal rejected |
| `IJobSystem` | engine core | 1 (implemented) | `JobSystem` thread pool, exceptions contained |
| `IRenderDevice` | renderer | 1 | Vulkan primary, OpenGL fallback |
| `IAssetProvider` | asset pipeline | 1 | async loading, handles, caching |
| `IPhysicsWorld` | physics | 1 | custom physics, documented determinism |
| `IAudioDevice` | audio | 1 | miniaudio behind a Poko interface |
| `INetworkTransport` | networking | 2 | no invented crypto or protocols |
| `IScriptRuntime` | MUTE | 3 | capability sandbox, no arbitrary OS access |
| `IPlatformServices` | runtime / backend | 3+ | client requests never prove validity |
| `IAccountProvider` | identity | 5 | backend validates tokens server-side |
| `IGameCatalog` | catalog | 5 | only approved release IDs are playable |
| `IPackageStore` | package delivery | 4/5 | immutable blobs, verified hashes |
| `IServerRegistry` | server registry | 5/6 | revocable credentials, heartbeats |

## Dependency rules (`systemplan/01`)

- Engine core must not depend on Cloudflare, Firebase, databases or UI.
- Backend must not depend on Dear ImGui.
- MUTE must not access arbitrary OS APIs.
- Game packages must not contain secrets.
- Untrusted games run in isolated environments.
- Final UI consumes stable application APIs and does not own business rules.

Enforced mechanically by: `poko_core` links no graphics, backend or UI
vendors; the two implemented seams live behind Poko-owned interfaces; the
`Server` configuration excludes windowed targets; and
`POKO_FETCH_DEPENDENCIES` is `OFF`.

## Core types (`engine/core`)

- Stable, immutable `EntityId` (64-bit) and generation-checked `TypedHandle`.
  Display names are never IDs (`systemplan/03`, `masterplan/26`).
- Structured `Logger` with levels, categories, UTC microsecond timestamps and
  a swappable sink for tests and future crash reporting. Log calls must never
  include tokens, personal data or sensitive moderation evidence
  (`systemplan/23`, rule 12 of `systemplan/30`).
- `POKO_ASSERT` exists only in `Debug`/`Development`; it compiles away in
  `Release`/`Server`.
- `poko::config` exposes the active configuration at compile time so
  development-only code cannot silently ship.

## Security design (draft)

Derived from `masterplan/17_SECURITY_THREAT_MODEL.md` and
`systemplan/30_GLOSSARY_AND_ENGINEERING_RULES.md`.

| Layer | Phase 0 control | Later control |
|---|---|---|
| Client | no privileged secrets; labelled stubs | signed updates, safe archive extraction |
| MUTE / runtime | capability seams declared, no implementation | instruction/memory/entity/network quotas, OS isolation |
| Server | headless `Server` configuration | unique identity, tickets, replay protection |
| Backend | no vendor dependency; no secrets in tree | least-privilege creds, rate limits, audit logs, backups |
| Packages | manifest schema draft, verifier STUB | signature/hash verification, size and path limits |
| Moderation | not started | isolated Review Runtime, restricted evidence, MFA |

Rules carried into every phase:

1. Use established cryptography and transport; never invent protocols.
2. Treat packages and network input as untrusted.
3. Never trust client-provided balances, permissions or review status.
4. Separate test and production environments.
5. Version persisted formats and APIs; keep immutable audit/ledger records.
6. Preserve third-party licences and notices.
7. No public release with exposed secrets, unverified signatures or untested
   recovery.

## Explicitly out of scope for Phase 0

Windowing/renderer devices, MUTE interpreter, networking, backend services,
package signing, moderation tooling and final UI. These arrive in the order
defined by `systemplan/27`.
