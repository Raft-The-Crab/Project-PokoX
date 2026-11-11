# 20 — Developer Hosting and .pkxx

## Hosting model
Poko provides the shared game platform/runtime integration. Developers provide hosting for their own games, except official Poko games initially hosted by Poko.

A headless server does not render graphics, audio or player UI, but it still runs authoritative simulation, game logic, physics relevant to gameplay, state validation, sessions and networking. Capacity must be benchmarked per game.

## Networking modes
Each game manifest may support:
- `offline`
- `player_hosted`
- `dedicated`

A game may support one or more modes. Offline play should not require an online account unless the game explicitly depends on online services.

## .pkxx contents
Game-specific package should contain:
- Manifest and compatibility range.
- Game-specific server scripts/data.
- Required server-side assets.
- Network configuration.
- Declared service permissions.
- Dependency/version manifest.
- Integrity hashes/signature.
- Resource defaults.

The shared Poko Dedicated Server Runtime should be installed and updated separately. Avoid bundling a duplicate runtime into every `.pkxx`.

## Server registration
- Server obtains identity through a secure registration flow.
- Credentials are short-lived or revocable and stored outside the package.
- Server sends heartbeat and version information.
- Poko registry tracks game ID, developer ID, runtime version, status and capacity.
- Player connection uses short-lived tickets validated by the server.

## Developer responsibilities
- Hardware and operating system.
- Internet connection and firewall.
- Server uptime and physical security.
- Host backups and local logs.
- Keeping runtime and game package updated.
- Protecting server credentials.

## Poko responsibilities
- Runtime distribution and signature verification.
- Package compatibility checks.
- Server registration/discovery services.
- Connection ticket issuance.
- Documentation and diagnostics.
- Abuse reporting and server revocation.
- Runtime security updates.

## Server operations
Suggested CLI:
- `poko-server doctor`
- `poko-server status`
- `poko-server logs`
- `poko-server restart`
- `poko-server update`
- `poko-server verify-package`

Do not promise player capacity before load tests using realistic scripts, physics, entities and network traffic.
