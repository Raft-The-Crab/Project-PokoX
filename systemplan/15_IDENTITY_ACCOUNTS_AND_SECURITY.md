# 15 — Identity, Accounts and Security

Use identity provider through backend token validation. Stable internal user IDs; usernames are changeable. Account data includes status, profile, contact verification, sessions, privacy and recovery state.

Security: short-lived access tokens, refresh rotation/revocation, secure credential storage, device/session list, revoke-all, MFA for moderators/privileged actions, login/recovery rate limits and audit logs.

Collect minimal personal data. Plan age-related and parental requirements with legal review for launch countries. Provide block/privacy controls and data export/deletion process.

Exit gate: account workflows work through API/CLI, sessions revoke correctly and no privileged secrets ship in clients.
