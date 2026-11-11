# 27 — Decision Log and Change Control

## Purpose
PokoX spans engine, platform, creator tools, economy, safety and infrastructure. Unrecorded decisions will cause incompatible implementations.

## Change process
1. Write the problem and affected systems.
2. Identify current confirmed decision.
3. List options and trade-offs.
4. Assess security, privacy, safety, performance and cost.
5. Record chosen option and owner approval.
6. Update impacted documents and APIs.
7. Add migration/backward-compatibility plan.
8. Add tests and acceptance criteria.
9. Communicate the change to developers and moderators if user-visible.

## Change categories
- Product policy.
- API/data schema.
- Engine/runtime.
- Security/privacy.
- Economy/monetization.
- Moderation and child safety.
- Infrastructure/hosting.
- UI/UX.

## Versioning
Use semantic versioning for public APIs and runtime interfaces where appropriate. Track document revision date and status. Keep old policy versions linked to historical moderation decisions.

## Decision template
### DEC-XXX — Title
- Date:
- Status: Proposed / Accepted / Rejected / Superseded
- Owner:
- Context:
- Options:
- Decision:
- Consequences:
- Security/privacy review:
- Migration plan:
- Related documents:

## Rule
A policy or schema change must not silently reinterpret an already-approved game release. Preserve the policy version, package hash and runtime version associated with each review.
