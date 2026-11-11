# 21 — MUTE Language and Sandbox

## Direction
MUTE begins as an interpreted language, with optimization later after the language semantics and runtime behavior are stable.

## Language design work
Specify:
- Syntax and grammar.
- Types and coercion rules.
- Scope and module system.
- Error handling.
- Async/event model.
- Standard library.
- Versioning and compatibility.
- Debugger and stack traces.
- Formatting and lint rules.
- Documentation and examples.

## Runtime APIs
Expose Poko-branded abstractions for engine functionality. Keep third-party implementation details behind internal interfaces, while preserving required licenses and notices.

## Capability security
Scripts receive explicit capabilities, for example:
- `player.read`
- `world.read`
- `world.modify`
- `storage.read`
- `storage.write`
- `network.send`
- `http.request`
- `audio.play`

A script should receive only the capabilities it needs. Sensitive permissions must be declared, reviewed and enforced at runtime.

## Execution limits
- Instruction/time budget.
- Memory budget.
- Maximum recursion/call depth.
- Entity/object creation quota.
- Event/message rate limits.
- Network request limits.
- Per-script and per-game quotas.
- Cancellation and timeout behavior.

## Library ecosystem
- Signed packages.
- Pinned versions.
- Permission manifests.
- Dependency graph.
- Vulnerability/revocation process.
- License metadata.
- No arbitrary native code in ordinary MUTE libraries.

## Developer tools
- Syntax highlighting.
- Autocomplete.
- Formatter.
- Linter.
- Debugger.
- Unit test runner.
- Profiler.
- API reference.
- Example templates.

## Security boundary
A language sandbox is not a substitute for OS/process isolation. Run untrusted game execution with operating-system restrictions as well.
