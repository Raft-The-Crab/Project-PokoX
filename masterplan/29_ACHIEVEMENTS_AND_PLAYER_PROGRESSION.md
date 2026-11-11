# 29 — Achievements and Player Progression

## Product model

PokoX has two separate achievement scopes:

1. **Game achievements:** authored by a game creator and earned inside that game.
2. **PokoX platform achievements:** authored by PokoX and earned through platform-wide activities.

Keep achievements separate from Pokoins. Achievement points are recognition/progression points, not currency and not redeemable for cash.

## Proposed launch limits per game

| Limit | Launch recommendation |
|---|---:|
| Total configured achievements | 100 |
| Hidden/secret achievements | 20 |
| Featured achievements on game page | 10 |
| Title length | 50 characters |
| Description length | 200 characters |
| Unlock per account per achievement | Once |

These are product defaults to validate through load tests and creator feedback. A platform-wide quota should also prevent one creator from creating excessive draft games or abusing achievement APIs. Consider raising limits only after storage, moderation and abuse data justify it.

## Achievement definition

Each achievement includes:

- Stable immutable achievement ID.
- Game ID and creator/team ownership.
- Localized title and description.
- Icon asset reference, verified and moderated as user-generated content.
- Category: Progression, Exploration, Skill, Collection, Social, Seasonal, Secret or Completion.
- Visibility: visible, hidden until earned, or secret with a hint.
- Unlock criteria represented through approved runtime events or server-validated conditions.
- Optional points value for display progression.
- Definition version, created/updated timestamps and lifecycle status.
- Optional season/expiry metadata with clear disclosure.

Editing title, description or icon must not erase earned progress. Material changes to criteria should create a new definition version or a new achievement, preserving the historical meaning of prior unlocks.

## Validation and trust

- Never trust a client-only `unlockAchievement` request as proof.
- Use server-authoritative events for online games.
- Offline games may queue local unlocks and synchronize later with an explicit offline-earned provenance marker; offline claims cannot always be proven cheat-free.
- Make unlock writes idempotent and unique on account + game + achievement ID.
- Apply rate limits and detect impossible event sequences or suspicious unlock bursts.
- Support test-mode achievements that cannot be granted to production accounts.
- Record unlock timestamps, definition version, provenance and validation status.
- Provide a correction/revocation process for confirmed fraud or erroneous definitions, with audit history and user-facing explanation where appropriate.

## Rewards

Default rewards are badges, titles, profile display, game-page showcase and achievement points. Do not automatically grant Pokoins. If a future achievement grants Pokoins, it must use the authoritative economy ledger, idempotency, fraud controls and the existing weekly earning cap. A game developer cannot mint Pokoins or create arbitrary platform-wide rewards.

## Player surfaces

- Game page: achievement count, categories and selected featured achievements.
- In-game notification: concise unlock toast with reduced-motion support.
- Player profile: selected achievements and optional showcase order.
- Achievement collection: filters for game, category, rarity label if supported, earned/unearned and date.
- Game details: hidden achievements remain concealed until unlocked; do not leak secret names through APIs or search indexes.
- Settings: control achievement popups, sound and profile visibility.

## Creator Studio surfaces

- Achievement list with search, filters and draft/published state.
- Create/edit form with live preview and localization fields.
- Criteria builder bound to safe runtime events; no arbitrary client-supplied trust.
- Test mode and simulated event viewer.
- Validation warnings for unreachable criteria, missing icons, duplicate IDs and unsupported event types.
- Usage analytics: unlock count and percentage using privacy-conscious aggregation; avoid exposing individual player histories to creators unless clearly necessary and permitted.
- Version history and rollback for definitions.

## Platform achievements

Examples include first game published, first group created, first helpful answer accepted, first event hosted and first contribution merged. Do not award achievements for spending money, mass messaging, issuing moderation actions, collecting reports or inviting large numbers of users. These can incentivize spam or harmful behavior.

## Open decisions

Confirm whether platform achievements award points, whether seasonal achievements can expire, whether players can hide all achievement activity, and whether game developers may award in-game cosmetic items through their own game economy. Keep the 100-per-game launch cap as a proposed default until confirmed.
