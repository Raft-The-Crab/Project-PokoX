# 12 — Game Runtime and Service API

Define game lifecycle, player/session events, world lifecycle, networking, persistence, match/server discovery, economy hooks, asset access, privacy-conscious analytics and optional platform capabilities.

Use interfaces such as `IPlayerService`, `IStorageService`, `INetworkService`, `IMatchService` and `IAnalyticsService`. Provide local test doubles first.

Separate client presentation from authoritative APIs. Client requests do not prove actions are valid.

Exit gate: same sample game uses documented APIs in offline and networked tests without direct UI/backend-vendor dependencies.
