# 05 — Groups, Teams and Collaboration

PokoX should distinguish **community groups** from **developer teams**. A group is a social/community space; a developer team owns or collaborates on projects. A group may later have a linked developer team, but their permissions should not automatically be identical.

## Community groups
Features:
- Create group with name, description, icon and banner.
- Public, private or invite-only visibility.
- Join requests and invite links.
- Member list and role badges.
- Group wall/announcements.
- Events and game recommendations.
- Group-owned curated game collections.
- Report group, report member and leave group.
- Group rules and moderation history.

Suggested roles:
- Owner
- Administrator
- Moderator
- Member

Permissions should be individually configurable. Owners can manage ownership and group deletion; administrators manage settings and membership; moderators handle group conduct; members participate according to privacy settings.

## Developer teams
Features:
- Team profile and verified ownership.
- Invite by account ID/handle.
- Project membership.
- Role assignment per project.
- Asset and source collaboration.
- Release approval permissions.
- Activity log and team announcements.
- Ownership transfer with confirmation and recovery process.

Suggested project roles:
- Owner: billing/ownership, team control, publishing authority.
- Admin: project settings and membership.
- Programmer: scripts and systems.
- Builder: scenes and world assets.
- Artist: models, textures and animation.
- UI Designer: interfaces.
- Tester: test access and bug reports.
- Publisher: can submit releases but not change team ownership.

## Permission model
Use least privilege. Permissions should be scoped to a project and action, not merely a broad role name. Examples:
- `project.read`
- `project.edit_scene`
- `script.edit`
- `asset.upload`
- `team.invite`
- `release.submit`
- `release.approve_internal`
- `analytics.read`
- `billing.manage`

Poko's platform moderator approval remains separate from a team's internal approval.

## Ownership and disputes
- Require explicit confirmation for ownership transfer.
- Maintain immutable ownership history.
- Provide recovery process for inaccessible team owners.
- Keep team member removal and project access revocation immediate.
- Preserve audit history after a member leaves.
- Do not let a team member download private project data after access is revoked.

## Anti-abuse
Rate-limit invitations, group creation and announcements. Detect spam groups, impersonation, harassment and coordinated invite abuse. Give users leave, mute and report controls.

## Launch recommendation
Start with basic groups, member roles, invites and developer team access. Add group walls, events and advanced collaboration after core publishing is reliable.
