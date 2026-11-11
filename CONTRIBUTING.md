# Contributing to PokoX

PokoX is **proprietary, closed-source software** owned by Raft-The-Crab (see
[LICENSE](LICENSE)). This is not an open-source project. Contributions are
accepted **only from explicitly authorised contributors**. Do not open public
pull requests or submit code, assets or content unless you have been granted
access and have the right to assign your work to Raft-The-Crab.

## Mandatory repository rules

These rules are authoritative. See also [CODEBUDDY.md](CODEBUDDY.md).

1. **Git identity.** Every commit and push must use:

   ```
   Raft-The-Crab <Galvion.ai@gmail.com>
   ```

   Never use any other author or committer identity, and never use a global or
   machine-local fallback identity.

2. **Pushing requires explicit instruction.** Never push. Only push when the
   repository owner explicitly instructs you to push in that specific context.
   Local commits are fine; pushing, force-pushing, and any operation that writes
   to the remote are not allowed without that instruction.

3. **No AI attribution.** Commit messages, pull requests, issues and tags must
   contain **no** AI-generated or AI-assistance wording of any kind. In
   particular, never add:

   - `Co-Authored-By:` trailers,
   - "Generated with ..." / "Written by ..." / "Assisted by ..." lines,
   - references to AI assistants, models, agents or automation,
   - any attribution other than the account above.

   Write plain, human-style commit messages.

4. **Never commit secrets.** No credentials, signing keys, service-account
   keys, tokens, `.env` files or production data in the repository, clients,
   game packages or developer-hosted game files.

## Development philosophy

PokoX follows a **system-first, sequential subsystem completion** model: build
the machinery before polished interfaces. See `systemplan/00` through
`systemplan/30` for the full engineering plan and `masterplan/` for product
scope.

Build order: engine foundation -> networking -> MUTE/runtime -> package build
pipeline -> backend -> dedicated server -> publishing/moderation -> remaining
platform services -> end-to-end integration -> final Player/Studio/Mod UI.

A subsystem is not "done" at the happy path. It needs documented
responsibilities, stable interfaces, error handling, tests, a security review,
diagnostics, performance measurements, documentation and reproducible builds.

### Implementation labels

Label every placeholder explicitly and never let a mock ship in production:

| Label | Meaning |
|---|---|
| `MOCK` | Test-only fake |
| `STUB` | Interface exists, implementation absent |
| `LOCAL_DEV` | Local-only, not production-safe |
| `PRODUCTION` | Implemented and reviewed for release |

## Technology by area

| Area | Stack |
|---|---|
| Engine / server core | C++20, CMake |
| Windows UI | Rust + egui |
| Android UI | Kotlin |
| Edge / backend functions | TypeScript (Cloudflare Workers) |
| Selected backend services | Go |
| Renderer | Vulkan primary, OpenGL fallback, Diligent Engine |
| Libraries | SDL2, GLM, miniaudio, Asio, Zstd, glTF, Dear ImGui (dev only) |
| Scripting | MUTE (interpreted, sandboxed) |

Package formats: `.pk` (Studio project), `.pkx` (player game package),
`.pkxx` (game-specific server package). Build configurations are separated
into `Debug`, `Development`, `Release` and `Server` (see `profiles/`).

## Engineering rules

From `systemplan/30_GLOSSARY_AND_ENGINEERING_RULES.md`:

1. Keep module boundaries explicit.
2. Engine core does not depend on backend or UI vendors.
3. UI does not own business rules.
4. Version persisted formats and APIs.
5. Use established cryptography and transport; never invent protocols.
6. Treat packages and network input as untrusted.
7. Never ship secrets in clients or packages.
8. Separate test and production environments.
9. Measure before optimizing or promising capacity.
10. Record architecture decisions and migrations.
11. Preserve third-party licenses and notices.
12. Make mocks impossible to confuse with production.
13. Keep a playable engine test scene.
14. Build final UI against stable APIs.

Dear ImGui and CLI/debug tooling are development-only and must be kept out of
engine core and excluded from release builds.

## Workflow

- Work on a feature branch; keep changes scoped to one subsystem.
- Write tests alongside implementation (unit, integration, contract, fuzz where
  relevant). Security-sensitive changes need a threat-model review.
- Update the relevant `systemplan/` or `masterplan/` document and the decision
  log when a decision or interface changes. Do not silently turn a proposed
  recommendation into a confirmed requirement.
- Update [CHANGELOG.md](CHANGELOG.md) under `Unreleased`.
- Keep a clean, reproducible build (`cmake --preset ...`) before requesting
  review; CI runs tests, formatting and static analysis.
- Preserve `LICENSE`, `NOTICE` and third-party notices; record any new
  dependency in a software bill of materials.

### Commit message format

Plain imperative subject, optional short body. No trailers, no attribution,
no AI references.

```
Add Vulkan swapchain recreation on device loss

Handle VK_ERROR_DEVICE_LOST by rebuilding the swapchain and invalidating
cached framebuffers. Covers the fallback path required by systemplan/04.
```

## Review checklist

- [ ] Builds cleanly in all four configurations.
- [ ] Tests added and passing; no mocks in production paths.
- [ ] No secrets, no untrusted package execution on a workstation.
- [ ] Interfaces documented; format/API versions bumped where needed.
- [ ] Accessibility, security and performance considered (QA is not deferred).
- [ ] CHANGELOG and planning docs updated.
- [ ] Commit uses the correct identity and contains no AI attribution.
