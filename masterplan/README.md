# PokoX Master Planning Pack

**Status:** Planning baseline  
**Prepared:** 2026-11-09  
**Purpose:** A structured, editable collection of Markdown documents for planning PokoX.

PokoX is a free-to-use creator gaming platform with a player client, Poko Studio, Poko Mod, a custom engine/runtime, creator-hosted game servers, and a moderation/publishing pipeline.

## How to use this pack
1. Read `00_MASTER_OVERVIEW.md` first.
2. Treat `01_CONFIRMED_DECISIONS.md` as the current decision register.
3. Use `02_OPEN_DECISIONS_AND_ASSUMPTIONS.md` to track unresolved choices rather than silently assuming answers.
4. Give each subsystem document to the person responsible for that area.
5. Update the version history and decision register whenever a decision changes.

## Document index
- `00_MASTER_OVERVIEW.md` — product vision, scope, principles and system map.
- `01_CONFIRMED_DECISIONS.md` — decisions already supplied by the project owner.
- `02_OPEN_DECISIONS_AND_ASSUMPTIONS.md` — unresolved questions and recommended defaults.
- `03_PRODUCT_SURFACES_AND_NAVIGATION.md` — player, Studio and Mod app areas.
- `04_ACCOUNTS_IDENTITY_AND_PRIVACY.md` — accounts, sessions, privacy and safety controls.
- `05_GROUPS_TEAMS_AND_COLLABORATION.md` — social groups and developer teams.
- `06_SOCIAL_AND_COMMUNITY_FEATURES.md` — friends, chat, profiles, follows and community.
- `07_GAME_DISCOVERY_AND_LIBRARY.md` — search, discovery, favorites, history and game pages.
- `08_CREATOR_ECONOMY_AND_POKOINS.md` — virtual currency, earnings, fees and abuse prevention.
- `09_MARKETPLACE_AND_ASSET_ECOSYSTEM.md` — assets, libraries, creator listings and licensing.
- `10_DEVELOPER_PORTAL_AND_ANALYTICS.md` — creator dashboards, metrics and publishing tools.
- `11_NOTIFICATIONS_AND_COMMUNICATIONS.md` — in-app notices, email/push and preferences.
- `12_SUPPORT_HELP_CENTER_AND_TUTORIALS.md` — support, docs and dedicated-server tutorials.
- `13_CLIENT_STUDIO_AND_PLATFORM_SETTINGS.md` — settings, accessibility and device support.
- `14_UPDATES_RELEASES_AND_COMPATIBILITY.md` — client/runtime updates and rollback.
- `15_INFRASTRUCTURE_OPERATIONS_AND_BACKUPS.md` — hosting, observability, backups and recovery.
- `16_API_AND_DATA_ARCHITECTURE.md` — service boundaries, data ownership and API conventions.
- `17_SECURITY_THREAT_MODEL.md` — security principles and threat controls.
- `18_MODERATION_PUBLISHING_AND_APPEALS.md` — formal review policy and workflow.
- `19_REVIEW_RUNTIME_AND_GAME_INSPECTOR.md` — isolated testing and inspection tools.
- `20_DEVELOPER_HOSTING_AND_PKXX.md` — developer-provided server model and `.pkxx`.
- `21_MUTE_LANGUAGE_AND_SANDBOX.md` — MUTE design and execution safety.
- `22_ENGINE_CLIENT_AND_GAME_RUNTIME.md` — engine architecture and client/server split.
- `23_ALPHA_BETA_AND_RELEASE_ROADMAP.md` — staged delivery plan and acceptance gates.
- `24_TESTING_QUALITY_AND_ACCESSIBILITY.md` — QA strategy and accessibility.
- `25_RISK_REGISTER_AND_METRICS.md` — major risks, mitigations and success metrics.
- `26_GLOSSARY_AND_FILE_FORMATS.md` — terms, package formats and naming.
- `27_DECISION_LOG_AND_CHANGE_CONTROL.md` — how future changes are governed.
- `28_COMMUNITY_CONTRIBUTION_AND_HELP_POKOX.md` — infrastructure support, volunteers, transparency and roadmap feedback.
- `29_ACHIEVEMENTS_AND_PLAYER_PROGRESSION.md` — game/platform achievements, limits, rewards and validation.
- `30_COMMUNITY_SAFETY_AND_TRUST.md` — safety controls, volunteer boundaries and trust.
- `31_DESIGN_SYSTEM_AND_VISUAL_LANGUAGE.md` — visual direction, themes, components, motion and app layouts.
- `32_COMMUNITY_AND_ACHIEVEMENT_UI_SPEC.md` — detailed community, Help PokoX and achievement screen layouts.
- `33_SIGNATURE_DIFFERENTIATORS.md` — PokoX product pillars and differentiators.
- `34_POKO_GAME_PASSPORT_AND_DISCOVERY.md` — transparent game details and player-respecting discovery.
- `35_CREATOR_WORKFLOW_AND_PORTABILITY.md` — guided Studio workflow, playtests, project export and analytics.
- `36_POKOX_LABS_AND_COMMUNITY_GOVERNANCE.md` — experiments, community roadmap, funding and volunteer governance.
- `37_DIFFERENTIATOR_ROADMAP_AND_PRIORITIES.md` — phased delivery priorities and differentiation scorecard.

## New proposed defaults
- Game achievements: proposed launch cap of 100 per game, including no more than 20 hidden and 10 featured. These are recommendations awaiting owner confirmation.
- Community funding and volunteering are separate from Pokoins and creator payouts. Contributions never buy moderation privileges.
- Visual direction: modern soft-rounded interface, high-quality dark/light themes, restrained animation and colourful 2008-web-inspired personality.

## Important planning notes
- This is a planning baseline, not legal advice, a security certification, or proof that any third-party service is suitable for production.
- Free-tier limits, product terms, pricing and availability can change. Recheck them before implementation.
- Never place production secrets in clients, game packages, public repositories or developer-hosted game files.
- Do not execute untrusted games directly on moderator workstations.
- Do not promise server capacity until representative benchmarks exist.
- PokoX content labels are disclosures, not permission to publish prohibited content.


## Differentiation strategy
PokoX differentiates through the combined execution of offline-capable play, creator-chosen hosting, an inspectable MUTE/Studio workflow, actionable human review, an integrated Community, a transparent Game Passport, player-respecting discovery, project portability and clear performance/compatibility information. These are product goals, not claims that every individual feature is unique to PokoX.

## Combined development plan
The sibling folder `PokoX_System_First_Plan/` in the final ZIP contains the 32-document system-first engineering plan. It defines the sequential build order: foundation, complete engine, networking, MUTE/runtime, packages, backend, hosting, publishing/moderation, platform systems, integration without final UI, then final UI. The master plan defines product scope; the system-first plan defines implementation order and subsystem exit criteria.

## Final archive structure
- `PokoX_Master_Plan/` — product, platform, community, economy, safety, UI and feature planning.
- `PokoX_System_First_Plan/` — system-first engineering plan and technical subsystem sequence.
