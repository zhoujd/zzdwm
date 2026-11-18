# Add SSE Streaming Output

Status: proposal (Tier 2)

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
