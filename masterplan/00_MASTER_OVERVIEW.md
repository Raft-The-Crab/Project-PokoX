# 00 — PokoX Master Overview

## Vision
PokoX is a free-to-use gaming and creation platform. It combines user-created games, block-building experiences, social discovery and creator tooling. Its product inspiration includes the accessibility and creator ecosystem of Roblox and the building creativity of Minecraft, while PokoX should establish its own identity, APIs, visual design and technical implementation.

## Main applications
1. **PokoX Player** — discover, download, launch and play games.
2. **Poko Studio** — create games, build scenes, write MUTE scripts, collaborate and publish.
3. **Poko Mod** — review submissions, run isolated tests, investigate reports and manage safety cases.
4. **Poko Dedicated Server Runtime** — shared headless runtime used with game-specific `.pkxx` packages. This is a runtime/tooling component, not necessarily a fourth consumer app.

## Core principles
- Free-to-use baseline; do not design features that require paid infrastructure to function.
- Offline-capable games should remain playable without login where technically and legally appropriate.
- Online features must be explicit and permission-controlled.
- Server authority protects important game state; clients are not trusted for rewards, inventory, purchases or competitive outcomes.
- Human moderators make final publishing decisions.
- Broad creative freedom is encouraged, while sexually explicit and exploitative content is prohibited.
- Child safety, privacy and security are foundational requirements, not optional launch polish.
- Build a small reliable product before adding a huge feature catalogue.

## Major system map
- Identity and authentication
- Player profiles and privacy
- Friends, groups and developer teams
- Game discovery and library
- Player client and engine
- Poko Studio and MUTE
- Game packages and dedicated server packages
- Developer-hosted game servers
- Publishing, review and moderation
- Economy and creator marketplace
- Notifications and support
- Operations, monitoring and security

## Suggested launch scope
The first public-facing release should be deliberately narrow:
- Windows player client.
- Windows Poko Studio.
- One or two official games: a block-building experience and an Obby.
- R6 default character support with extensible attachment points.
- Basic game discovery, profiles, favorites and reporting.
- Basic publishing forms, package validation, isolated review testing and human approval.
- Basic MUTE interpreter with strict sandbox limits.
- Developer-hosted dedicated server workflow for a limited set of supported modes.
- Essential account security, privacy controls and moderator audit logging.

Android support should follow once platform abstractions, performance budgets, input design and packaging are stable. Android 11+ is the stated target, but verify current device support and store requirements before release.

## Non-goals for the first release
- A full Roblox-scale economy.
- User-uploaded arbitrary native code.
- A large public asset marketplace with cash payouts.
- Advanced recommendation algorithms.
- Automatic final moderation decisions.
- Global low-latency hosting operated by Poko.
- Guaranteed anti-cheat or reverse-engineering prevention.

## Product success definition
PokoX is ready to expand when players can reliably discover and play a small number of safe games, creators can build and publish without hand-holding, moderators can inspect submissions safely, and the platform can operate within known infrastructure and moderation capacity.


## PokoX product identity
PokoX should stand out through a coherent combination rather than a claim that one feature is unprecedented:
- Offline-capable play where a game supports it.
- Creator choice over player-hosted and developer-hosted dedicated servers.
- An inspectable, beginner-friendly Studio and sandboxed MUTE workflow.
- Human publishing decisions with actionable findings and an isolated review environment.
- Poko Community integrated into the platform.
- A Game Passport explaining content, connectivity, accessibility, privacy and monetization.
- Discovery filters and user controls rather than engagement-only ranking.
- Project backup/export and explicit asset licensing information.
- Achievement capacity that starts at 100 per game and can be expanded without a purchase ceiling, 10 slots at a time for 150 Pokoins per game.
