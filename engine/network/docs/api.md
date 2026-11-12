# network

`api::INetworkTransport` seam. Two implementations, separate targets so a MOCK
is never confused with production (systemplan/30 rule 12):

- `poko_net_null` — **MOCK** `NullNetworkTransport`
  (`include/poko/network/transport.h`). Eager loopback, no framing.
- `poko_net_channel` — **PRODUCTION** `ChannelNetworkTransport`
  (`include/poko/network/channel_transport.h`). A real in-process datagram
  protocol implementation using only the C++20 standard library.

`ChannelNetworkTransport` implements the parts the MOCK omits: a versioned
`PKXN` frame header (`magic | channel | flags | sequence | length`, see
`src/channel_detail.h`), per (destination, channel) sequence numbers, a
`Connecting -> Connected` handshake pumped by `update()`, reliable-ordered
delivery that restores ascending sequence even when frames arrive shuffled,
deterministic loss for unreliable channels, and generation-checked endpoint
lifetime. Frames addressed to a not-yet-connected endpoint are deferred until
the handshake completes. `set_reorder()` / `set_drop_every()` are deterministic
test hooks; `framed_count()`, `reordered_count()`, `dropped_count()` expose
protocol counters.

**Deferred:** actual OS socket I/O. Asio is `NOT VENDORED` in
`third_party/DEPENDENCIES.md` with a `TBD` revision/hash; pinning it requires a
real immutable revision, SHA-256 and licence notice, which must not be guessed
(see the deferral recorded in `systemplan/28`). A socket backend will put the
same `PKXN` frames on the wire.

Public headers live under `include/poko/network/`; sources under `src/`;
tests under `tests/`.
