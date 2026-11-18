# Add Session Save/Resume

Status: proposal (Tier 1)

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
