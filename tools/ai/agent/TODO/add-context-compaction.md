# Add Context Compaction

Status: done (2026-11-23)

Goal:

- MAX_MESSAGES 24 silently drops the oldest messages today — on long
  tasks that deletes the original task statement. Instead, fold the
  messages about to be dropped into an endpoint-generated summary
  message, pinned ahead of the plan, so continuity survives pruning.
- `/compact` lets the human trigger it deliberately between subtasks.
- Compaction failure degrades to today's drop-with-a-note; it must
  never cost the session.

Implementation sketch:

- Automatic trigger at the pruning site (when dropping would remove at
  least one message) plus the `/compact` command.
- One-off summarize request to the same endpoint, sent buffered (not
  streamed — it is internal, nothing should hit the screen), with a
  fixed system prompt: keep the task goal, decisions made, files
  touched, open threads.
- The reply is inserted as a pinned message ahead of the plan system
  message; kept messages follow unchanged, so compaction itself never
  splits a tool_call/tool_result pair (pairs still drop together, as
  today).
- `.ai/session.jsonl` gains a compaction record so `--continue` resumes
  the compacted conversation rather than resurrecting the bloat;
  torn-line tolerance is unaffected.
- Auto-compaction is rate-limited (at most once per few rounds) since
  it costs a request; the [usage] context number already exposes the
  growth that motivates it.
- Summarize-request tokens join the cumulative usage totals; the exit
  line can note `compacted ×N`.
- Roughly 250 lines in ai.c. Tests: scripted summarize reply, next
  request contains the summary and not the dropped messages, pairing
  preserved, manual /compact, transcript record plus resume, and
  endpoint-failure fallback to plain pruning.
