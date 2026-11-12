# 28 — Open Decisions and Risks

## Open decisions
- Four review attempts per game lifetime or per release?
- One appeal per submission, game or enforcement case?
- Final content label vocabulary.
- Launch age policy and parental controls by region.
- Which messaging/social features ship initially?
- Offline account requirements.
- Supported dedicated server OS/network modes.
- Paid marketplace timing.
- Pokoins transferability/cash equivalence.
- Reward eligibility, caps and total budget.
- Production hosting and paid fallback strategy.
- Pin miniaudio / Asio (immutable revision, SHA-256, licence notice) before adding
  OS audio output or socket I/O. Both are `NOT VENDORED` in
  `third_party/DEPENDENCIES.md`; `29` lists them only as "ecosystem considered".
  Until pinned, engine backends implement deterministic, stdlib-only subsystem
  logic behind the existing seams and defer vendor/OS I/O. No version or hash
  may be guessed (see the pinning policy in `DEPENDENCIES.md`).

## Risks
Overbuilding engine before playable validation; free hosting instability; custom physics/protocol defects; package migration problems; unsafe untrusted execution; debug UI shipping accidentally; accessibility feedback arriving too late; backend services proliferating without ownership.

## Mitigations
Limit engine scope to first official games; keep a playable test scene; benchmark; version formats/APIs; use established crypto/transport; isolate untrusted code; continuously test security; prototype user workflows before final UI.
