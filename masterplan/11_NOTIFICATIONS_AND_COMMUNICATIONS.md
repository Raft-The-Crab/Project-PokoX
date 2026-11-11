# 11 — Notifications and Communications

## Notification channels
- In-app notification center.
- Push notifications on Android, if enabled.
- Desktop notifications on Windows, with user permission.
- Email for essential account/security and support communications, if an email provider is configured.

## Notification categories
- Account security.
- Friend/group invitations.
- Group announcements.
- Game update notices.
- Developer team activity.
- Publishing review results.
- Appeals and moderation actions.
- Server outage and runtime compatibility warnings.
- Support ticket updates.
- Official Poko announcements.

## Controls
- Per-category preferences.
- Quiet hours.
- Disable promotional notices.
- Unsubscribe from non-essential email.
- Collapse repetitive events.
- Mark read/unread.
- Deep link to the relevant game, project or case.

## Reliability
Use an outbox pattern: commit the business event and notification intent together, then deliver asynchronously. Make delivery idempotent so retries do not create duplicates.

## Safety
Do not include sensitive moderation evidence, private chat content or authentication secrets in notifications. Use generic wording and require the user to sign in to view protected details.

## Anti-spam
Rate-limit invitations and announcements. Provide notification mute controls for groups, users and games.
