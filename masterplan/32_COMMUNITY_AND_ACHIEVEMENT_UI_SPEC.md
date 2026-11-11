# 32 — Community, Help PokoX and Achievement UI Specification

## Community hub

### Desktop layout
- Header with PokoX identity, global search and account actions.
- Left navigation: Home, Discussions, Q&A, Groups, Events, Creators, Roadmap and Help PokoX.
- Main feed: featured announcement, recommended discussions, recent posts and event cards.
- Right rail: upcoming events, active groups and community guidelines.

### Mobile layout
- Bottom navigation for Home, Discover, Community, Notifications and Profile.
- Community sub-navigation as tabs or a compact menu.
- Feed cards stack vertically; avoid three-column desktop patterns on phones.

### Post composer
Support text, permitted images, game links, project links, polls and category selection. Show audience/visibility before posting. Provide preview, draft save, report, edit history and delete confirmation.

## Groups

Group landing page includes banner/icon, name, description, join/request button, rules, member count, tabs for Posts, About, Events and Projects, plus visible role labels for staff. Group settings include membership, roles, moderation, appearance, connected projects and audit history. Never make social membership grant Studio project permissions implicitly.

## Creator profiles and portfolios

Profile header includes avatar, display name, short bio, badges and follow/friend actions according to privacy settings. Portfolio sections include published games, projects, approved assets, tutorials and contribution highlights. Do not expose private email, real name, precise location or account security data.

## Help PokoX page

Use a hero panel explaining what community support makes possible. Below it, show:
1. Infrastructure goals with target, raised/spent/remaining, recurring cost and last update.
2. Ways to help: volunteer, contribute skills, sponsor hosting, provide credits or support a named funding goal.
3. Volunteer role cards with duties, eligibility, expected time and application button.
4. Transparency reports and completed-goal archive.
5. FAQ, rules, contact and safeguarding information.

Funding progress must be factual and timestamped. Clearly label estimates and pending contributions. Never show a fake live counter or imply funds are secured before settlement.

## Volunteer application flow

Role selection → role description and eligibility → application form → privacy notice and consent → confirmation → status tracker. Collect only role-relevant details. Provide application states: Draft, Submitted, Under Review, Interview/Task, Training, Probation, Accepted, Waitlisted, Declined and Withdrawn. Do not reveal internal reviewer notes.

## Roadmap and feedback portal

List and board views with category filters, search, status, official response, vote count and update date. Detail view includes problem statement, discussion, changelog and subscribe action. Rate-limit votes and make moderation/reporting controls available.

## Achievement collection

Player view includes a game selector, earned/locked tabs, category filters, achievement cards, progress where supported and secret placeholders that reveal only safe hints. Detail view shows unlock date, game, description and reward. User can choose profile showcase and popup preferences.

## Studio achievement manager

List view includes achievement icon, title, category, visibility, unlock count, version and status. Editor includes metadata, localization, icon preview, criteria builder, test event simulator, validation and publish controls. Warn when a condition is unreachable or depends on untrusted client input. Provide a test-mode banner that cannot be mistaken for a real production unlock.

## Moderation and admin views

Community moderation queues show assigned items, context, policy category, action history and escalation controls. Sensitive safety cases use separate permissions and interfaces. Infrastructure contribution records and volunteer records are separate from moderation decision screens to prevent perceived influence or conflicts of interest.
