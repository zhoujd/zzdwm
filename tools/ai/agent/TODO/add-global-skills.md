# Add ~/.ai/skills Global Skills [DONE]

Status: done (2026-11-26)

Shipped inside ai.c's skills code (no new module): the Skill struct
carries its base directory, read_skill and /skill re-read through it,
load_skills_from loads .agents/skills then ~/.ai/skills with the
project winning collisions on name or directory alias, and the skills
system message names both locations with the real home path plus the
-C caveat sentence. The full suite (173 tests, two new) passes.

Goal:

- A user-level skills directory `~/.ai/skills/<name>/SKILL.md` with the
  same format as the project's `.agents/skills`, so personal skills
  (release checklists, host-specific runbooks) apply in every project
  without copying directories around.
- The project wins on collision: a global skill whose directory name
  matches an already-loaded skill's name or directory is skipped, and
  the shared 16-skill budget fills project skills first.
- A missing HOME or unreadable `~/.ai/skills` is silent, exactly like a
  project without `.agents/skills` — fresh installs and cron see no
  change.

Implementation sketch:

- read_skill takes the base directory (today it hardcodes SKILLS_DIR
  twice: once at load, once when /skill re-reads the body), and the
  Skill struct gains that base so handle_skill_command opens the file
  it actually came from.
- load_skills loads `.agents/skills` first, then `$HOME/.ai/skills`
  with a skip for defined names, and sorts once after both; the
  banner, /skills, and the model's skills system message pick up
  globals for free.
- The skills system message header names both locations with the real
  home path (the model reads these files), and gains one sentence for
  the -C case: user-level skills live outside the workspace, so there
  the model asks the human to run `/skill <name>`, which injects the
  body the agent itself reads.
- Tests: HOME at a temp dir — a global skill reaches the banner and
  the /skill injection; a project skill with the same directory name
  wins; existing no-HOME runs keep loading project skills only.

Not in this change: `--init-config` creating `~/.ai/skills` (the
directory is created on demand by the user; init stays about the
config file), and per-skill location markers in the /skills listing
(the header carries both paths).
