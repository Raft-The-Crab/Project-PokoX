# 30 — Glossary and Engineering Rules

Engine: reusable low-level systems. Game Runtime: executes game logic. Headless: no graphics/audio/UI presentation, but still may simulate and validate. Review Runtime: isolated untrusted-game environment. Game Inspector: diagnostics for scene/assets/scripts/services/performance. MUTE: interpreted scripting language. `.pk`, `.pkx`, `.pkxx`: project, player package and server package. Mock: fake test dependency. Stub: unimplemented interface. ImGui: temporary development/debug UI.

## Rules
1. Keep module boundaries explicit.
2. Engine core does not depend on backend or UI vendors.
3. UI does not own business rules.
4. Version persisted formats and APIs.
5. Use established cryptography and transport.
6. Treat packages and network inputs as untrusted.
7. Never ship secrets in clients or packages.
8. Separate test and production environments.
9. Measure before optimizing or promising capacity.
10. Record architecture decisions and migrations.
11. Preserve third-party licenses/notices.
12. Make mocks impossible to confuse with production.
13. Keep a playable engine test scene.
14. Build final UI against stable APIs.
