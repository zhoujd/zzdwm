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
`--resume`, plan restore, torn-line tolerance);
add-file-tools.md (read_file / list_dir / apply_patch in tool.c, workspace
confinement with symlink resolution, approval prompt for patches);
add-sse-streaming.md (`"stream":true` + `text/event-stream` handling in
receive_stream, deltas reassembled into the buffered response shape,
`AI_STREAM=0` opt-out, main-thread-only output);
add-landlock.md (landlock.c direct-syscall ruleset in the exec child:
workspace rw, /tmp scratch, system trees ro, `AI_LANDLOCK=0` opt-out,
graceful degradation to the lexical filter).

## Tier 2 — bigger bets

(none proposed; see "Deliberately out of scope" for the boundaries)

## Deliberately out of scope

Plugins, embeddings/RAG, GUI, config-file formats beyond the existing
`AI_*` env vars — each pulls against the static-musl, one-binary design.

## Sandbox status note

`-C` now layers three boundaries: the lexical path filter on command
strings, realpath-based confinement in the file tools, and Landlock
filesystem rules on every approved exec (5.13+; degrades to the lexical
filter with a warning on older kernels, `AI_LANDLOCK=0` opts out).
Still out of scope: seccomp, network restrictions, chroot, setuid.
