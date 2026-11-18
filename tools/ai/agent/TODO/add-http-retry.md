# Add HTTP Retry with Backoff [DONE]

Status: complete and validated. `call_endpoint` is now a retry loop around `call_endpoint_once`: HTTP 429 and 5xx responses plus refused/reset connections are retried up to 3 attempts with exponential backoff (1s, 2s), an integer `Retry-After` header overrides the delay (capped at 60s), and every retry prints `[retrying in Ns (attempt N/3)]`. Other 4xx, timeouts, and interruptions are never retried; a failed attempt mutates nothing.

Goal:

- Retry the API request on transient failures: HTTP 429, 5xx, and
  connection-reset before any bytes are streamed.
- Respect `Retry-After` when present, otherwise exponential backoff
  (e.g. 1s, 2s, 4s; 3 attempts max).
- Makes long agent runs reliable over flaky links / rate limits without
  the human babysitting and re-prompting.

Implementation sketch:

- Single loop around the existing request function; no state to unwind,
  since a failed request mutates nothing (conversation is only appended
  on success).
- Never retry 4xx other than 429 — those are protocol/model errors that
  a retry cannot fix.
- Print `[retrying in 2s (attempt 2/3)]` to stderr so the human sees the
  stall.
