# Claude Code in Docker

Pinned Claude Code environment on the internal zhoujd/ubuntu:latest image, so
`claude` runs on any host with docker and no local node install. Config comes
from the host: `~/.claude` (settings, key, sessions) and `~/.claude.json`
(onboarding/key approval) are mounted in, so the containerized claude behaves
like the host one — the GLM proxy is reached over the LAN, and the
region-blocked api.anthropic.com preflight is skipped by the patched
`~/.claude.json` (see `misc/vibe`).

## Build

```
make -C ../dockerfiles claude-base
make -C ../dockerfiles claude-base CLAUDE_VERSION=2.1.323   # pin a version
```

## Run

```
cd <any project>
../claude/run.sh              # interactive TUI, cwd mounted as the workspace
../claude/run.sh -p "say ok"  # headless print mode
../claude/run.sh shell        # bash via docker exec into the running TUI container,
                              # or a fresh container when none is running
```

The container runs unprivileged as user `zach` (uid 1000) with host
networking; files it creates in the mounted workspace are owned by uid 1000.

## Notes

- The image user is uid 1000. On hosts where the user's uid differs, files
  created inside the workspace will be owned by 1000.
- `~/.claude.json` is a single-file bind mount; claude rewrites it in place.
- To share git identity with the container, extend `run.sh` with
  `-v $HOME/.gitconfig:/home/zach/.gitconfig`.
