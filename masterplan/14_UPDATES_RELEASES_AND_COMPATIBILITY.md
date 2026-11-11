# 14 — Updates, Releases and Compatibility

## Separate release tracks
- Player client.
- Poko Studio.
- Poko Mod.
- Engine/runtime.
- Dedicated Server Runtime.
- Game packages.
- MUTE standard libraries.

## Release channels
Suggested channels:
- Development/internal.
- Alpha.
- Beta.
- Stable.

Use signed manifests and package hashes. Clients should verify updates before installation.

## Rollout
- Small internal cohort.
- Limited beta cohort.
- Gradual stable rollout.
- Pause switch if crash or error rates spike.
- Rollback to last known-good client/runtime where safe.

## Game release process
A developer submits a new immutable game version. The currently published version remains active while the new one is reviewed. A human moderator approves the exact package hash and version. Publishing must atomically point the game to the approved release.

## Compatibility
Each package declares:
- Package format version.
- Minimum and maximum supported runtime versions.
- Required services.
- Supported operating systems/platforms.
- MUTE language version.
- Dependency versions.
- Networking modes.

## Breaking changes
- Deprecate APIs before removal where feasible.
- Publish migration guides.
- Keep compatibility windows.
- Test old packages against new runtimes.
- Do not silently change game behavior through an unannounced runtime update.

## Emergency revocation
Allow Poko to revoke a malicious or dangerous package. Offline clients may not receive revocation immediately; communicate this limitation and use short-lived online authorization for sensitive online capabilities.
