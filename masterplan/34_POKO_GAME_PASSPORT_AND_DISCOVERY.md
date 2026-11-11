# 34 — Poko Game Passport and Player-Respecting Discovery

## Game Passport

Every published game receives a structured information panel. It should be visible on the game detail page and accessible before launch.

### Sections

1. **About:** title, summary, screenshots, trailer, developer and release notes.
2. **Content:** suitability labels, descriptions and prominent warnings.
3. **How it runs:** offline, player-hosted, developer-hosted dedicated server, or combinations.
4. **Devices and controls:** supported OS/device classes, keyboard/mouse, controller and touch support.
5. **Accessibility:** text scaling, remapping, subtitles, colour options, reduced motion and other supported features.
6. **Privacy and connectivity:** account requirement, chat/voice, analytics, cloud save and network features.
7. **Monetization:** purchases, subscriptions if ever supported, random rewards if permitted, and game-specific economy disclosures.
8. **Technical details:** version, package size, runtime compatibility, last update and known issues.
9. **Community:** official group, Q&A, events, bug reports and developer announcements.
10. **Hosting:** responsible operator, server status and measured evidence where available.

### Trust labels

Use distinct labels:
- Developer declared.
- Automatically checked.
- PokoX verified.
- Not yet verified.
- Not applicable.

Never show a developer declaration as a PokoX verification. Explain what each verification checks and when it was last performed.

## Discovery system

### Search and filters

- Text search across game title, description, tags and creator.
- Genre and subgenre.
- Offline availability.
- Server mode.
- Device compatibility.
- Control scheme.
- Accessibility features.
- Content labels.
- Session length, if developer supplied or safely estimated.
- Recently updated.
- New releases.
- Community activity, with anti-manipulation protections.

### Recommendation controls

- Explain why a game is recommended.
- Let players tune or reset recommendation preferences.
- Offer non-personalized discovery.
- Provide a chronological new/recently updated feed.
- Do not treat time spent as the only quality signal.
- Do not sell undisclosed organic ranking.
- Avoid using sensitive personal data for recommendations.

### Game page layout

- Hero artwork, title, developer and clear Play button.
- Short summary and Game Passport preview.
- Screenshots/media and gameplay tags.
- Server/offline compatibility.
- Content labels.
- Updates and known issues.
- Achievements and selected community activity.
- Reviews with spam and manipulation protections.
- Similar games and curated collections.

### Reviews and feedback

- Limit fake, duplicate and coordinated reviews.
- Distinguish a public review from a private bug report.
- Let developers reply without deleting legitimate criticism.
- Show review sorting and moderation rules.
- Do not reward positive reviews with Pokoins.
- Provide a report and appeal path for review moderation.

## Acceptance criteria

- Players can determine whether a game is offline-capable before downloading.
- Players can understand key content labels and network requirements.
- Unverified developer declarations are not presented as verified facts.
- Search filters work with keyboard, touch and assistive technology.
- Recommendation controls are discoverable and functional.
- Sponsored placement, if ever introduced, is clearly labeled and separated from organic ranking.
