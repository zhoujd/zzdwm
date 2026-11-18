# Add File Tools (read/list/apply_patch)

Status: proposal (Tier 2)

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
  symlinks) in one stroke.

Implementation notes:

- Single best architectural upgrade available; keeps tools declarative and
  auditable.
- `apply_patch` should refuse to touch files outside the workspace
  regardless of `--full-access` unless explicitly widened.
