# Ledger Ingest Protocol v1

This protocol lets any bridge (HTTP today, TCP/ZeroMQ later) feed prices into the Pi alert engine without redesigning backend logic.

## 1) Goals

- Idempotent ingest with dedupe (`source_id + sequence`)
- Replay support for dropped/out-of-order messages
- Heartbeat-based source state (`ACTIVE`, `STALE`, `OFFLINE`)
- Transport-agnostic payloads (works over HTTP/TCP/ZeroMQ)

## 2) Message Schema

Each event has:

- `source_id` string: stable producer ID (example: `mt5-main-01`)
- `sequence` integer: strictly increasing per `source_id` (start at 1)
- `event_type` enum: `price` or `heartbeat`
- `symbol` string: required for `price` (example: `EURUSD`)
- `bid` number optional
- `ask` number optional
- `mid` number optional (preferred)
- `ts_ms` integer optional (producer timestamp in ms)
- `checksum` string optional (payload hash)

Price selection order in backend:

1. `mid`
2. `(bid + ask) / 2`
3. `bid`
4. `ask`

## 3) HTTP Envelope (Current)

POST `/ingest/events`

Headers:

- `Content-Type: application/json`
- `X-Ledger-Key: <INGEST_SHARED_KEY>` (required when server is configured)

Body:

```json
{
  "protocol_version": 1,
  "transport_id": "http-bridge-1",
  "events": [
    {
      "source_id": "mt5-main-01",
      "sequence": 101,
      "event_type": "price",
      "symbol": "EURUSD",
      "bid": 1.08231,
      "ask": 1.08235,
      "mid": 1.08233,
      "ts_ms": 1770000000123,
      "checksum": "sha256:..."
    },
    {
      "source_id": "mt5-main-01",
      "sequence": 102,
      "event_type": "heartbeat",
      "ts_ms": 1770000001123
    }
  ]
}
```

Response:

```json
{
  "protocol_version": 1,
  "accepted": 2,
  "duplicates": 0,
  "rejected": 0,
  "sources": {
    "mt5-main-01": {
      "highest_sequence": 102,
      "highest_contiguous_sequence": 102,
      "missing_sequences": []
    }
  },
  "server_time": "2026-05-06T00:00:00+00:00"
}
```

## 4) Replay Contract

If `missing_sequences` is non-empty, producer should replay those sequence IDs.

POST `/ingest/replay-request`

```json
{
  "source_id": "mt5-main-01",
  "from_sequence": 90,
  "to_sequence": 120
}
```

Response includes exact missing IDs and current watermarks.

## 5) Dedupe Rules

- Primary idempotency key: `(source_id, sequence)`
- Duplicate sequences are accepted as duplicates and ignored for alert re-triggering
- Invalid payloads go to deadletter with reason

## 6) Heartbeat and Failover States

Recommended producer behavior:

- Send heartbeat every 3 to 5 seconds per `source_id`
- Keep sequence monotonic across both `price` and `heartbeat`

Server state transitions (configurable):

- `ACTIVE`: heartbeat age < `HEARTBEAT_STALE_SECONDS`
- `STALE`: heartbeat age >= `HEARTBEAT_STALE_SECONDS`
- `OFFLINE`: heartbeat age >= `HEARTBEAT_OFFLINE_SECONDS`

Read status at GET `/ingest/status`.

## 7) Transport Mapping (Future)

Keep payload exactly same when moving to TCP/ZeroMQ:

- TCP: newline-delimited JSON batch frames
- ZeroMQ PUB/SUB: topic + JSON batch payload

As long as event schema and sequence semantics stay unchanged, backend does not need redesign.

## 8) Producer Implementation Notes

- Persist last sequence on disk per `source_id`
- Batch up to 100 to 500 events/request depending on latency target
- Retry with exponential backoff on network errors
- On reconnect, call replay endpoint and resend missing IDs first
