# 01 — Confirmed Decisions

This document records decisions explicitly provided by the project owner. Recommendations elsewhere should not silently override them.

## Product and platform
- Product name: **PokoX**.
- Main applications: PokoX Player, Poko Studio and Poko Mod.
- Target platforms: Windows 11 for all three apps; Android 11+ for Player and Studio; Poko Mod is Windows-only.
- Languages: C++20 engine/server components, Rust + egui for Windows UI, Kotlin for Android UI, TypeScript on Cloudflare Workers, and Go for selected backend services.
- First official game directions: block-building and Obby.
- Game modes: offline, player-hosted and dedicated-server modes can be supported depending on the game.
- R6 is the initial default character rig; custom accessories are supported. R15 and other rigs may follow.
- Engine and client development should proceed together.
- MUTE begins as an interpreter and can be optimized later.

## Infrastructure currently considered
- Cloudflare R2, Workers and D1.
- Firebase Authentication.
- Cloudinary.
- Layerbase services.
- Aiven Valkey/Redis and PostgreSQL.
- Hugging Face free CPU hosting for early experiments or non-critical services, with known reliability limitations.
- No project funding is currently available; community support may be considered if PokoX gains traction.

These are the services the project currently has or is considering, not a guarantee that they are appropriate for production. Recheck quotas, terms, availability and data-processing conditions before relying on them.

## Economy and creator model
- Pokoins are a platform virtual currency.
- Earned Pokoins from tasks have a stated cap of 1,500 per week, with a weekly reset.
- Developer reward concept: 5 Pokoins per 3 minutes of eligible play in a developer's game.
- Platform fee on eligible developer revenue: 15%; developer share: 85%.
- Pokoins must not be represented as cash or guaranteed redeemable value unless a separate, legally reviewed cash-out programme is explicitly created.

## Packages and hosting
- `.pkx`: player game package.
- `.pk`: Poko Studio project/share file.
- `.pkxx`: game-specific server package.
- Poko provides the game system/runtime and platform integration; third-party developers provide hosting for their games.
- Poko-operated servers are initially for official PokoX games.
- Server runtime is headless and does not render graphics/audio/UI, but it still simulates or validates authoritative game state.

## Moderation decisions
- Every game update must undergo review before the updated version is published.
- Content suitability is represented through descriptive content labels.
- A human moderator makes the final approval decision.
- Developers get one appeal opportunity for a formal rejection.
- Four total formal reviews: one initial review plus three retries.
- PokoX broadly allows creative genres, including limited fictional blood, horror and violence, but prohibits sexually explicit/adult content and child sexual exploitation.
- Moderation includes game execution/testing, not merely reviewing reports.

## Important scope ambiguity
The owner confirmed four formal reviews but has not explicitly confirmed whether the four-attempt allowance is:
1. lifetime per game, or
2. per release/update submission.

This pack does not silently settle that question. See `02_OPEN_DECISIONS_AND_ASSUMPTIONS.md`. A practical recommendation is four attempts per release submission, with game-level enforcement for repeated or serious violations.

## Community and achievements
- Poko Community is integrated directly into PokoX as a native platform feature; it is not a separate standalone product or separate identity/account system.
- Every game includes 100 achievement slots.
- Developers may permanently purchase 10 additional achievement slots for 150 Pokoins. There is no limit on the number of expansion purchases. Each purchase applies only to the specific game selected, is non-transferable to other games, and remains attached to that game.
