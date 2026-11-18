# Add SSE Streaming Output

Status: done (2026-11-18)

Goal:

- Stream assistant output token-by-token (Server-Sent Events) instead of
  buffering the complete JSON response.
- Biggest perceived-speed win: first text appears in ~1 round-trip, and
  long answers no longer look hung mid-generation.
- TUI mode gets incremental rendering; `-e` mode can print tokens raw.

Implementation sketch:

- Hand-parse `data:` frame lines and incrementally decode delta chunks;
  cJSON does not help with the stream itself, so this is a medium-effort
  custom parser over the existing socket read loop.
- Accumulate deltas into the final assistant message so conversation
  state is identical to today's buffered path.
- Must handle `[DONE]`, keep-alive comments, and mid-stream errors.

Status notes:

- `build_request` appends `"stream":true` unless `AI_STREAM=0`.
  `receive_response` sniffs `Content-Type: text/event-stream` on a 2xx
  reply and delegates to `receive_stream`; a plain JSON answer falls back
  to the buffered path unchanged.
- `receive_stream` feeds raw body bytes through a small state machine
  (dechunk with its own size-line buffer, SSE line split, `data:` event
  accumulation) and parses each event with cJSON: `delta.content` is
  appended to a shared buffer, `delta.tool_calls[]` fragments are joined
  by index into id/name/arguments, `usage` is captured when sent, and
  `delta.reasoning_content`, `event:`/`id:`/`retry:` fields, and
  keep-alive comments are ignored. `[DONE]` or a clean close finishes the
  stream; the accumulated state is reassembled into the exact buffered
  response JSON, so `process_response`, conversation state, and session
  logs are unchanged.
- Output discipline: the worker thread only accumulates; the main thread
  is the sole stdout writer, flushing new deltas between live-steering
  polls (100 ms) and once after the worker joins, so streamed text never
  interleaves with steering markers. The TUI's existing partial-line
  pending buffer renders deltas as they arrive.
- The API timeout becomes per-quiet-period while streaming (every recv
  resets it). Once any token reached the screen the attempt is final —
  no retry replays visible output.
- Nine tests: header-once text, split tool-call arguments, buffered
  fallback, comment/field tolerance, mid-stream error, close without
  `[DONE]`, streamed usage into the exit summary, `AI_STREAM=0` request
  shape, and TUI rendering.
