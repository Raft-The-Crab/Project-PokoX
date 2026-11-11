# 31 — PokoX Design System and Visual Language

## Design direction

PokoX should combine modern product usability with the cheerful, expressive visual energy of colourful 2008-era gaming websites. The result should feel nostalgic and playful, not dated, cluttered or difficult to navigate.

Principles:
- Soft rounded surfaces and friendly geometry.
- Clear hierarchy, generous spacing and readable typography.
- Colourful accents, gradients and occasional playful illustrations.
- Dark and light themes with equal design quality.
- Subtle motion that supports feedback and orientation.
- Dense information only where creator/moderator workflows require it.
- Consistent components across web, Player, Studio and Mod while respecting each device.

## Visual ingredients

- Rounded cards, panels, dialogs and inputs; use a consistent radius scale rather than rounding everything excessively.
- Bright accent colours such as sky blue, violet, coral, mint, yellow and pink, balanced with neutral surfaces.
- Soft shadows and restrained borders; avoid excessive glow and glass effects.
- Friendly icons with consistent stroke weight and clear labels.
- Occasional pixel-inspired motifs, sticker-like badges, playful empty states and retro game illustrations. Keep body text and controls modern.
- Use gradients sparingly for hero areas, featured events and major calls to action.

## Theme tokens

Define semantic tokens rather than hardcoding colours per screen:

- `bg.canvas`, `bg.surface`, `bg.elevated`, `bg.subtle`.
- `text.primary`, `text.secondary`, `text.muted`, `text.onAccent`.
- `border.default`, `border.strong`, `focus.ring`.
- `accent.primary`, `accent.secondary`, `accent.success`, `accent.warning`, `accent.danger`, `accent.info`.
- `state.hover`, `state.pressed`, `state.disabled`, `state.selected`.

Dark mode should use layered charcoal/navy surfaces rather than pure black everywhere. Light mode should use warm off-white or very light neutral surfaces rather than harsh white everywhere. Check contrast in both themes. Never rely on colour alone for status.

## Typography and layout

- Use a highly legible UI sans-serif family with clear numeric forms.
- Establish a compact type scale for captions, body, section headings and page titles.
- Keep line lengths comfortable for reading.
- Use an 8-point spacing rhythm with a small set of exceptions for compact controls.
- Use responsive content widths and clear breakpoints.
- Keep primary navigation predictable and secondary navigation contextual.

## Motion

Use short, restrained transitions for hover, press, selection, drawers, tabs, achievement unlocks and page transitions. Provide reduced-motion support and avoid motion that delays user input. No constant bouncing, flashing, parallax-heavy backgrounds or animations that obscure content. Important status changes must also be conveyed with text and accessible semantics.

## Component library

Build shared components for buttons, icon buttons, inputs, search, selects, tabs, segmented controls, cards, list rows, badges, avatars, tooltips, menus, dialogs, toasts, progress bars, skeleton loaders, empty states, error states, tables, pagination, file upload, media preview, navigation and breadcrumbs.

Each component defines default, hover, focus, pressed, disabled, loading, selected, error and success states as applicable. Document keyboard behavior and responsive behavior.

## Product surface layouts

### PokoX website and community
- Top bar: PokoX logo, Discover, Community, Create, Help PokoX, search, notifications and account menu.
- Community home: featured announcement, trending discussions, groups to discover, events, creator showcase and roadmap highlights.
- Help PokoX: infrastructure goals, volunteer programmes, contribution opportunities, transparency reports and progress updates.
- Game pages: large game identity area, play/install action, creator, content labels, media, description, achievements, updates and reviews.

### PokoX Player
- Desktop: compact left navigation rail/sidebar, central discovery or game content, optional right-side social/context panel.
- Mobile: bottom navigation for primary destinations, top-level search, full-screen game details and sheet-based secondary actions.
- Prioritize Play, Continue, Discover, Library and Friends. Keep settings and less-used tools out of the primary action area.

### Poko Studio
- Desktop-first workspace: top command bar, left project/navigation panel, central editor viewport, right inspector/properties panel, bottom output/problems/console panel.
- Panels can resize, collapse and restore layouts.
- Preserve work state and autosave; show save/sync state clearly.
- Use dense but readable data tables and code editors; do not force oversized rounded cards into professional editing surfaces.
- Mobile Studio should prioritize project management, lightweight editing, asset review and collaboration rather than pretending every desktop workflow fits a phone.

### Poko Mod
- Professional operations console with persistent navigation, review queue, assigned work, report queue, appeals, safety cases, audit logs and system health.
- Use high information density, strong status labels, clear evidence provenance and visible reviewer identity.
- Keep sensitive case views visually distinct and permission-gated.
- Confirm destructive or irreversible actions and record audit events.

## Interaction rules

- Primary action is visually obvious; secondary actions are quieter.
- Do not use colour-only labels; pair status colour with text and/or icon.
- Show loading, empty, offline, permission denied, validation error and success states.
- Use optimistic updates only when rollback and error feedback are reliable.
- Avoid modal dialogs for routine navigation; prefer inline panels and drawers.
- Make destructive actions explicit and reversible where possible.
- Preserve keyboard navigation, visible focus, screen-reader labels, touch target sizes and text scaling.

## Accessibility and performance

Support reduced motion, keyboard-only use, contrast checks, scalable text, descriptive icons, captions for media and accessible forms. Avoid excessive blur, large animated backgrounds and unbounded feed rendering. Virtualize long lists and lazy-load media. Test both themes, narrow screens and low-end Android devices.

## Visual acceptance checklist

- The same semantic colour means the same thing across all apps.
- Dark and light themes both pass contrast checks.
- Components have documented interaction states.
- No important action is hidden behind hover-only behavior.
- Motion can be reduced or disabled.
- The 2008-inspired personality appears through colour, illustration, badges and small details, not clutter or obsolete interaction patterns.
