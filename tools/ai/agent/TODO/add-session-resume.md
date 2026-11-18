# Add Session Save/Resume [DONE]

Status: complete and validated. Non-system messages, tool results, and plan
updates append to `<workspace>/.ai/session.jsonl` one JSON object per line,
flushed per message; `--continue` replays the latest session and
`--resume <file>` a specific one, in interactive, one-shot, and TUI modes.
Replay re-adds stored messages verbatim through the normal MAX_MESSAGES
window, restores the active plan, skips torn or malformed lines, and
`/new` restarts the transcript so `--continue` resumes what was on screen.

Goal:

- Append the transcript to `<workspace>/.ai/session.jsonl` and add
  `--continue` (resume latest) / `--resume <file>`.
- Biggest quality-of-life win for iterative coding sessions: restart the
  binary (or the machine) without losing conversational context.

Implementation sketch:

- cJSON is already available for serializing message objects; JSONL keeps
  the file append-only and crash-tolerant.
- On resume, replay stored messages into the conversation before the first
  model request; respect MAX_MESSAGES pruning as usual.
- File lives inside the workspace so `-C` scoping contains it naturally.
- Open question: include tool results verbatim (fat) or summarized?
  Resolved: store the already-bounded versions the conversation holds, so
  replay costs no more context than the live session did.
