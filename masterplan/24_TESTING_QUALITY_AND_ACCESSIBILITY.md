# 24 — Testing, Quality and Accessibility

## Test pyramid
- Unit tests for math, serialization, economy ledger and policy rules.
- Integration tests for APIs, databases, package delivery and server registration.
- End-to-end tests for login, download, launch, publish and review.
- Fuzz tests for package parsers and network protocols.
- Security tests for permissions and sandbox boundaries.
- Manual QA for gameplay, moderation and accessibility.

## Required test suites
### Client
- Install/update/uninstall.
- Offline launch.
- Package corruption.
- Controller/keyboard/touch input.
- Graphics fallback.
- Low-memory behavior.
- Crash recovery.

### Studio
- Autosave/recovery.
- Import/export.
- MUTE compilation/interpreter errors.
- Project collaboration permissions.
- Package build reproducibility.
- Publish and rollback behavior.

### Server
- Registration and revocation.
- Invalid tickets.
- Disconnect/reconnect.
- Load and stress behavior.
- Resource limits.
- Persistence and recovery.

### Moderation
- Four-attempt enforcement.
- Invalid review not consuming an attempt.
- Appeal isolation.
- Evidence access control.
- Test environment cleanup.
- Approval tied to exact package hash.
- Emergency suspension.

## Accessibility
- Keyboard-only operation.
- Text scaling.
- Color contrast.
- Reduced motion.
- Captions/subtitles.
- Remappable controls.
- Clear error language.
- Touch target sizing on Android.

## Release gates
No public release with unresolved critical security vulnerabilities, known data-loss bugs, unreviewed production secrets, or a broken account recovery path.
