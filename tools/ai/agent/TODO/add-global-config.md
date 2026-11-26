# Add ~/.ai/config Global Settings [DONE]

Status: done (2026-11-26)

Shipped as a config.c/config.h module (extracted after an inlined
first draft perturbed ai.c's inlining enough to trip format-truncation
warnings on unrelated code): config_load/config_value/config_init.
Keys are exactly the environment names, the environment outranks the
file and flags outrank both, unknown keys and malformed lines are
startup errors, and --init-config scaffolds a fully commented 600
template inside a 700 ~/.ai that refuses to overwrite. The full suite
(171 tests, including twelve new config ones) passes.

Goal:

- A user-level settings file `~/.ai/config`: env-style `KEY=VALUE` lines
  with `#` comments and blank lines, keys identical to the existing env
  vars (AI_API_URL, AI_MODEL, AI_API_KEY, AI_TIMEOUT_SECONDS,
  AI_COMMAND_TIMEOUT_SECONDS, AI_TOOL_ROUNDS, AI_STREAM, AI_LANDLOCK).
  Same vocabulary as the environment means zero new settings surface.
- Precedence: CLI flags > environment > ~/.ai/config > built-in
  defaults. The environment is per-invocation so it outranks the file;
  today's code already layers flags over env (`-m`, `-t`), and the file
  slots underneath at the existing getenv sites.
- One place to point every interactive `ai` on the machine at the lab
  endpoint, model, and timeouts without shell-profile exports. The file
  lives outside every working tree, so no new .gitignore entries;
  sessions and job logs stay in `<scope>/.ai` on purpose (per-project
  resume, sandboxed self-inspection, concurrency isolation).

Implementation sketch:

- One loader in ai.c runs before the per-key reads in
  configure_environment: HOME unset or ~/.ai/config missing/unreadable
  means no file keys and no message (cron/daemons and fresh installs
  behave exactly as today). Lines: trim, skip blank and leading `#`,
  split on the first `=`, trim both sides, reuse strip_quotes so a
  quoted URL works. Duplicate key: last wins.
- Each getenv ("AI_...") in configure_environment becomes
  setting_value ("AI_..."): env first, then the table. Every value then
  flows through the existing validation, so a bad file value fails with
  the same message its env var would print.
- Unknown key in the file is a hard error naming the key: a typo
  silently ignored is a setting that quietly never applies. Keys the
  file never accepts: AI_SUBAGENT (internal parent-to-child marker)
  and OPENAI_API_KEY (env fallback) stay environment-only.
- The API key in the file is plaintext like its env equivalent; README
  says create it 0600, and the loader warns once when AI_API_KEY is
  present and the file is group- or world-readable (one stat call).
- Roughly 100 lines in ai.c: the loader plus the getenv replacements
  in configure_environment. No cJSON, no new format surface — which is
  what un-does the roadmap's "config-file formats beyond the existing
  AI_* env vars" out-of-scope line: the file speaks the same keys.
- Tests: HOME pointed at a temp dir (the landlock test's pattern) —
  file value reaches the banner, env beats file, flag beats env,
  missing file is silent, invalid value and unknown key error out.
- `--init-config` scaffolds the file: fully commented template, mode
  600 inside a mode 700 ~/.ai (private by construction, so the
  permission warning cannot fire for a file ai itself created), and an
  existing file is never overwritten.

Follow-on candidate once the file exists (not this change):
`~/.ai/skills/` as a global skills directory beside the project's
`.agents/skills`, project wins on name collision.
