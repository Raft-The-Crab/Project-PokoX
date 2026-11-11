# 12 — Support, Help Center and Tutorials

## Help Center
Provide documentation inside Poko Studio and on the PokoX website. Keep docs versioned with the engine/runtime release.

Suggested sections:
1. Getting Started.
2. Installing Poko Studio.
3. Building a first Obby.
4. Building a block world.
5. Scene and asset management.
6. Character rigs and animation.
7. MUTE language basics.
8. MUTE libraries and permissions.
9. UI editor.
10. Poko services.
11. Offline games.
12. Player-hosted multiplayer.
13. Dedicated server hosting.
14. `.pkx`, `.pk` and `.pkxx` formats.
15. Publishing and review rules.
16. Content labels and safety.
17. Performance optimization.
18. Debugging and troubleshooting.
19. Team collaboration.
20. Security best practices.

## Dedicated server tutorial
The tutorial should guide developers through:
1. Select dedicated mode in Studio.
2. Configure player limits and server requirements.
3. Build the game-specific `.pkxx`.
4. Download the compatible Poko Dedicated Server Runtime.
5. Install runtime on a supported host.
6. Configure the package and environment.
7. Create/register a server identity through Poko.
8. Keep credentials outside the package and source repository.
9. Open only required network ports.
10. Start the server and run `poko-server doctor`.
11. Verify registration and heartbeat.
12. Test a player connection.
13. Configure restart, logs and backups.
14. Update runtime and package safely.

## Diagnostics CLI
Suggested commands:
- `poko-server doctor`
- `poko-server status`
- `poko-server logs`
- `poko-server restart`
- `poko-server update`
- `poko-server verify-package`

Commands should explain failures and give safe next steps. Do not print secrets in logs.

## Support system
- Bug report form with app version, OS and optional diagnostic bundle.
- Feature request board with status.
- Developer support tickets.
- Account recovery path.
- Safety report path distinct from ordinary technical support.
- Status page for outages.

## Documentation quality
Every tutorial should include prerequisites, expected result, screenshots, common errors, security notes and tested version. Provide beginner and advanced paths.
