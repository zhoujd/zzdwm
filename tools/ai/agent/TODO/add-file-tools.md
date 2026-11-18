# Add File Tools (read/list/apply_patch) [DONE]

Status: complete and validated. `read_file` / `list_dir` / `apply_patch` are
built-in tools in a new tool.c module (tool.h interface), declared to the
model alongside `run_terminal_command` and `plan`; the dispatcher in ai.c
owns prompts and message building. Reads need no approval and stream the
same numbered lines to the human that the model receives; every path is
confined to the workspace root (symlinks followed, `realpath` prefix check)
even under `--full-access`. `apply_patch` takes whole-line SEARCH/REPLACE
hunks (aider/V4A style: path line, `<<<<<<< SEARCH`, exact lines, `=======`,
replacement, `>>>>>>> REPLACE`; empty SEARCH creates, empty REPLACE deletes;
each SEARCH must match exactly one location), passes a pre-approval scope
check, then asks `[a]llow, [s]teer without applying, [n]o, [f]orce, [q]uit`.
Multi-file sequential application reports which files were already patched
when a later hunk fails. Full-access mode auto-applies; refusal reports the
human's decision to the model.

Goal:

- Add `read_file(path)`, `list_dir(path)`, and `apply_patch(hunks)` as
  built-in tools alongside `terminal`.

Wins:

- Large token savings: `read_file` returns exact lines without `cat` +
  shell quoting round-trips; `apply_patch` (V4A-style hunks) is far
  cheaper than heredoc rewrites and is diffable/reviewable.
- Sandbox enforcement becomes argument validation instead of fragile text
  scanning of shell command lines — eliminates the known bypass classes
  (`$(...)`, `$HOME` expansion, base64-encoded payloads, out-of-workspace
  symlinks) for everything routed through the file tools.

Implementation notes:

- Single best architectural upgrade available; keeps tools declarative and
  auditable.
- `apply_patch` refuses to touch files outside the workspace regardless of
  `--full-access` (pre-approval check, so a scope-violating patch is never
  half-applied).
