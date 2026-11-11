# 04 — Accounts, Identity and Privacy

## Identity model
Use a stable internal Poko user ID that is not the username. Usernames can change; internal IDs should not. Store account status, creation date, verified contact methods, security state and privacy settings separately.

## Authentication
- Firebase Authentication may be used as an initial identity provider.
- Backend services validate tokens server-side.
- Use short-lived access tokens and secure refresh-token rotation/revocation.
- Never embed service-account keys, database credentials or signing private keys in clients.
- Support sign-out all devices, session list and session revocation.
- Require MFA for moderators and privileged developer actions.
- Provide recovery flows resistant to account takeover.

## Profiles
Potential public fields:
- Display name and unique handle.
- Avatar and profile description.
- Public games and creator links.
- Optional badges and group memberships.
Private fields:
- Email/contact details.
- Birth/age information if collected.
- Security settings.
- Moderation and support case data.

Do not expose private profile data through public APIs.

## Privacy controls
- Profile visibility.
- Who can send friend requests.
- Who can message the user.
- Who can invite the user to groups.
- Who can see online status and activity.
- Block and mute lists.
- Data export and account deletion request process.
- Marketing/notification preferences.
- Analytics choices where applicable.

## Child safety and age
PokoX is intended for an audience that may include children. Establish age-appropriate defaults, restrict adult contact patterns, and obtain legal advice on age thresholds, parental consent and regional obligations. Do not collect more age or identity data than necessary. Avoid promising that a self-declared birthday proves age.

## Account security features
- Login alerts for new devices.
- Password reset and recovery.
- Suspicious login detection.
- Session/device management.
- Rate limits on login and recovery.
- Username change cooldown and impersonation controls.
- Optional passkeys if supported by the chosen identity stack.

## Data minimization
Collect only data needed for platform operation, safety and user-requested features. Define purpose, access roles, retention and deletion for every sensitive data category.
