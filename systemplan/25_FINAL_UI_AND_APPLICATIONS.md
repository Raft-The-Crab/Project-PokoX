# 25 — Final UI and Applications

Final UI begins after system contracts and end-to-end workflows stabilize. UI is deferred, not dismissed: it still needs usability, accessibility and performance work.

Player: discovery, search, game pages, library, profiles, friends, groups, notifications, settings and play.

Studio: project browser, scene/asset/character/animation tools, MUTE editor, UI editor, collaboration, test, publish and help.

Mod: review queue, Game Inspector, Review Runtime controls, reports, appeals, safety cases, developer actions and audit history.

Windows UI planned with Rust + egui; Android with Kotlin. UI consumes stable APIs; business rules stay in domain/runtime/backend layers. Rebuild user-facing workflows, not debug-only controls.
