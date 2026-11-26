# Add Named Sessions (-S)

Status: done (2026-11-27)

Goal:

- Add `-S, --session <name>`: persist the transcript to
  `<workspace>/.ai/<name>.jsonl`, so parallel work streams keep
  separate histories instead of sharing one `.ai/session.jsonl`.
- Flip the default to no-session (ephemeral): a plain run writes no
  transcript and never creates `.ai/` — scratch directories, read-only
  workspaces, and `git status` stay clean. Persistence becomes
  opt-in, exactly like the difference between an incognito tab and a
  named one.
- `--continue` keeps its "resume the latest saved session" contract:
  with `-S <name>` it resumes that session, without it the most
  recently modified `.ai/*.jsonl` (a legacy `session.jsonl` counts).
- `--resume <file>` is unchanged: replay that file and keep appending
  to it — an explicit path plays the same role as a name.

Implementation sketch:

- One static `session_name[MAX_NAME]`; `session_open` returns before
  `mkdir (".ai")` when no session is active. Every writer (per-message
  append, `/new` restart, compaction rewrite) already funnels through
  that one choke point, so ephemeral runs need no other guards.
- Name validation at parse time: empty, longer than MAX_NAME (128),
  containing `/`, or equal to `.`/`..` is a usage error, exit 2 — the
  name becomes a filename under `.ai/`, so path traversal must not
  reach it. `--resume` keeps its own length check for explicit paths.
- `--continue` without `-S` scans `.ai/` for the newest `.jsonl` by
  mtime (opendir/stat, no glob dependency). Nothing found means a
  fresh ephemeral run; today's "starting fresh" line still applies,
  and there is no file to append to either.
- Subagents already write no transcripts (the depth guard), and job
  logs create `.ai/jobs/` only when a job actually starts, so a
  default run creates nothing anywhere.
- TUI: parse and forward `-S <name>` beside `--continue`/`--resume`,
  bump the child argv budget, add the usage line.
- Tests: a default run leaves no `.ai/` behind; `-S work` writes
  `.ai/work.jsonl` and `-S work --continue` round-trips; `--continue`
  alone picks the newest file; a bad name exits 2. The existing
  transcript tests gain `-S session` (read_session already takes a
  name).

Open questions:

- When `--continue` alone finds nothing, stay ephemeral (proposed) or
  recreate the old always-on `.ai/session.jsonl`? Proposed: stay
  ephemeral — `-S` is the single opt-in for persistence.
- Session listing and cleanup (`-S --list`, pruning stale files) would
  be its own proposal; not bundled here.

Status notes:

- Both open questions resolved as proposed: unnamed `--continue` with
  nothing found stays ephemeral; listing/cleanup was not bundled.
- `session_name`/`session_path` statics; `session_open` returns before
  any `mkdir` when no session is active, and the `mkdir (".ai")` is
  scoped to paths starting `.ai/` so an external `--resume` file never
  creates a stray directory. Jobs still mkdir `.ai/jobs` only inside
  `job_spawn`; subagents write nothing (depth guard).
- `valid_session_name` (length 0 caught as "requires a name", >= 128,
  any `/`, or `.`/`..`) is a usage error, exit 2; `-S` and `--resume`
  are mutually exclusive in both orders. The interactive banner gained
  a `Session:` line (the path, or `ephemeral (-S <name> saves it)`).
- `find_newest_session` scans `.ai` with opendir/readdir/stat: `.jsonl`
  suffix via `strrchr`, `S_ISREG`, strict `>` comparison so the first
  entry wins mtime ties (one-second resolution, noted in the comment).
- `--continue -S <name>` with a missing file starts fresh but still
  records (the name is the opt-in); `--continue` alone with nothing
  found stays ephemeral and prints
  `[session] no previous session in .ai; starting fresh`.
- TUI parses and validates `-S` early (a bad name cannot silently kill
  the backend), forwards it beside `--continue`; the child argv budget
  grew to `15 + 2 * MAX_ATTACHMENTS` (31 slots, exactly the maximum
  fill).
- Tests: full suite green (184 tests). New: default run leaves no
  `.ai` and prints the ephemeral banner; `-S work` writes
  `.ai/work.jsonl`; `-S work --continue` round-trips; missing named
  session starts fresh and records; `--continue` alone picks the
  newest of two sessions (alpha's mtime pinned to epoch 1 to defeat
  one-second resolution); bad names and both `-S`/`--resume` orders
  exit 2; TUI `-S` forwarding via tmux. Existing transcript tests run
  with `-S session` (same `.ai/session.jsonl` path, so `read_session`
  and the resumed-from assertions are unchanged), and the `list_dir`
  test counts 3 entries now that a default run creates no `.ai/`.
