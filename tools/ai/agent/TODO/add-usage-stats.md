# Add Usage Stats [DONE]

Status: complete and validated. `record_usage` sums `usage.prompt_tokens` / `usage.completion_tokens` per round and keeps the latest prompt size; the interactive agent prints `[usage] tokens: 12.4k in / 3.1k out, 7 rounds, context 3.2k` at exit, one-shot mode prints it after each round, and totals survive `/new`.

Goal:

- Print token usage at exit (interactive) / after each round (`-e`):
  prompt + completion tokens and round count, e.g.
  `tokens: 12.4k in / 3.1k out, 7 rounds`.
- The API response already carries a `usage` object and cJSON is already
  linked, so this is a small, low-risk diff.
- Makes the MAX_MESSAGES=24 drop-oldest window and prompt-growth visible
  instead of invisible; pairs with the printed session cost estimate.

Implementation sketch:

- Record `usage.prompt_tokens` / `completion_tokens` per round, sum
  cumulatively.
- Print in the existing summary/banner path; keep it off stderr.
