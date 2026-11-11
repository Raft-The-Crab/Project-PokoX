# 18 — Moderation, Publishing and Appeals

## Confirmed policy
- Every game update must be formally reviewed before the new version is published.
- Human moderators make final approval decisions.
- Automated checks assist but do not approve or reject independently.
- Games use descriptive content labels.
- One appeal opportunity is provided for a formal rejection.
- Four total formal reviews: initial review plus three retries.
- PokoX allows broad creative genres but prohibits sexually explicit/adult content and child sexual exploitation.

## Attempt-scope clarification
The owner has confirmed four total formal reviews, but the exact scope remains to be confirmed: lifetime per game or per release submission. The implementation must make the scope configurable until this is decided. Recommended default: four attempts per release submission, while repeated abuse can trigger game-level enforcement.

## Submission workflow
1. Developer completes forms and submits immutable `.pkx` and applicable `.pkxx`.
2. Validate package structure, hashes, signatures, manifests and compatibility.
3. Run automated scans.
4. Place submission in moderator queue.
5. Moderator inspects package and test results.
6. Moderator launches isolated Review Runtime.
7. Moderator records findings and decision.
8. Approved version is published atomically.
9. Rejected version receives actionable feedback.
10. Developer may retry within the allowance or submit one appeal.

## Update safety
- Never overwrite the published package during review.
- Keep last approved version available during ordinary update review.
- Emergency suspension is separate from update rejection.
- Each approval references exact package hashes and version.
- Every update goes through the formal review workflow.

## Content labels
Potential labels:
- Horror
- Blood
- Violence
- Gore
- Strong Language
- Scary Sounds
- Flashing Lights
- User-Generated Content
- Social Interaction

Labels are disclosures, not exemptions. Prohibited content remains prohibited even if declared. Final wording and label definitions require policy review.

## Prohibited content
- Pornography and sexually explicit games.
- Explicit sexual images, animations or interactions.
- Sexual exploitation or sexualization of minors.
- CSAM and grooming/sexual solicitation.
- Content or mechanics designed to facilitate exploitation.

Suspected child sexual abuse material must be handled through a specialized restricted safety process, not ordinary moderator browsing. Preserve evidence only under approved procedures, restrict access and follow applicable legal reporting obligations.

## Review findings
Each finding contains:
- Severity.
- Category/policy reference.
- Exact package version.
- Scene/object/script/asset location where possible.
- Description.
- Evidence reference.
- Required corrective action.
- Whether it blocks approval.

## Appeals
- One appeal per defined scope (scope to be finalized).
- Appeal must state which finding/decision is disputed.
- A different reviewer handles it where possible.
- Appeal outcome and rationale are recorded.
- An appeal does not automatically publish the game.
- Technical faults or moderator errors invalidate a review rather than count as a developer failure.

## Enforcement
Possible actions: warning, required update, temporary removal, publishing suspension, game termination, developer suspension or account termination. Actions must be proportionate, documented and appealable where appropriate.
