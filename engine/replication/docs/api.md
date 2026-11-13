# replication

`poko_replication` — **PRODUCTION**, standard library only, backend-agnostic
(systemplan/10). Runs over any `api::INetworkTransport`.

- `protocol.h` — `PKXR` v1 wire format: Hello/HelloReply handshake with version
  check, delta `Snapshot`, `Ack`, `InputCommand`. Decoders are total over
  arbitrary bytes (truncation, trailing bytes, bad magic, unknown mask bits,
  non-finite floats and oversized counts are rejected before allocating).
- `ReplicationServer` — server-authoritative entity table, per-client interest
  radius (nearest first, capped by `max_entities_per_snapshot`), delta
  snapshots against the last acknowledged tick (full snapshot when no usable
  baseline), input validation (handshake required, strictly increasing
  sequence, move vector clamped to `max_input_magnitude`), silent-client
  timeout.
- `ReplicationClient` — applies snapshots (drops stale ones and deltas whose
  baseline is missing), acks each applied tick, `sample()` interpolates
  position/velocity/yaw between buffered ticks and clamps outside the buffer.
- `pump()` drains the transport once and routes packets to the `PacketSink`
  owning the destination endpoint.

Trust boundary: the server trusts the `ClientId` ↔ endpoint binding made by
`add_client`. A socket backend must derive it from the authenticated
connection; the in-process transport cannot tell senders apart.

**Not yet done:** client-side prediction/reconciliation, reconnect, encryption
(never hand-rolled; needs an established library), cross-process LAN sample
(blocked on the deferred socket backend, see `engine/network/docs/api.md`).
