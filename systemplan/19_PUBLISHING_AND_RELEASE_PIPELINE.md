# 19 — Publishing and Release Pipeline

Workflow: developer forms; build immutable `.pkx`/optional `.pkxx`; validate; automated static checks; formal review case; human test; approval tied to exact hashes; atomically publish approved release; rejection returns actionable findings; retry/appeal according to policy.

Every update is reviewed. Previous approved version remains live during ordinary review unless emergency safety suspension is needed.

Confirmed: four formal reviews total (initial plus three retries). Scope (per game lifetime or per release) remains open. Poko-caused review infrastructure failure should not consume an attempt.

Exit gate: backend enforces duplicate submission, race, appeal, retry and package-substitution rules.
