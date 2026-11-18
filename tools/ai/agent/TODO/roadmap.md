# Roadmap

Proposals for the next features. Grounded in the current set: tools =
`terminal` + `plan`; flags `-C`, `--full-access`, `--ui`, `-f` (up to 8),
`-m`, `-e`; skills support; MAX_MESSAGES 24; default 128 rounds.

## Tier 1 — high value, small diffs

Done: add-workspace-banner.md (banner line + working-directory system message);
add-command-timeout.md (`-t` override, 64 KiB tail-truncated capture);
add-http-retry.md (3 attempts, backoff, `Retry-After`);
add-usage-stats.md (`[usage]` line, cumulative tokens + rounds + context);
add-session-resume.md (`.ai/session.jsonl` transcript, `--continue` /
`--resume`, plan restore, torn-line tolerance).

## Tier 2 — bigger bets

- add-file-tools.md — read_file / list_dir / apply_patch tools
- add-landlock.md — OS-level enforcement for `-C` (kernel 5.13+)
- add-sse-streaming.md — token-by-token streaming output

## Deliberately out of scope

Plugins, embeddings/RAG, GUI, config-file formats beyond the existing
`AI_*` env vars — each pulls against the static-musl, one-binary design.

## Sandbox status note

`-C` is a lexical path filter (validate_workspace_command, ai.c ~2286),
not OS isolation: no seccomp/landlock/chroot/setuid anywhere. Known
bypasses: variable expansion, command substitution, encoded commands,
symlinks pointing outside the workspace. add-file-tools.md and
add-landlock.md are the two complementary fixes.
