# 29 — Confirmed Product Decisions

## Applications
Player: Windows 11 and Android 11+. Studio: Windows 11 and Android 11+. Mod: Windows 11. C++20 engine/server, Rust + egui Windows UI, Kotlin Android UI, TypeScript Workers and Go services.

## Engine/game
Vulkan primary, OpenGL fallback; SDL2, GLM, miniaudio, Asio, Zstd and glTF ecosystem considered; custom physics; R6 first with accessory attachments, R15 later; MUTE interpreter first; block-building and Obby first official games; offline/player-hosted/dedicated modes per game.

## Packages/hosting
`.pk` Studio project/share; `.pkx` player game; `.pkxx` game-specific server package. Developers host their games; Poko supplies shared runtime/platform integration and initially hosts official games.

## Economy
Pokoins task earning cap proposal 1,500/week; developer engagement reward proposal 5 per 3 minutes; eligible revenue split proposal 15% platform / 85% developer.

## Moderation
Every update reviewed; content labels; human final approval; one appeal; four formal reviews (initial plus three retries); no sexually explicit/adult content or child sexual exploitation.

## Infrastructure considered
Cloudflare R2/Workers/D1, Firebase Auth, Cloudinary, Layerbase, Aiven PostgreSQL/Valkey and Hugging Face free CPU. Recheck quotas and production suitability.

## Current development direction
System-first sequential subsystem completion. Final UI is deferred until systems and end-to-end workflows function. ImGui, CLI and test harnesses are permitted during development.
