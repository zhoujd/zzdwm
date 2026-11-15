# Codex CLI in Docker

Pinned Codex environment on the internal `zhoujd/ubuntu:latest` image, so
`codex` runs on any host with Docker and no local Node.js installation. Host
configuration and the current workspace are mounted into the container.

## Build

```
make -C ../dockerfiles/tool codex-base
make -C ../dockerfiles/tool codex-base CODEX_VERSION=0.169.0   # pin a version
```

## Run

```
cd <any project>
../codex/run.sh             # interactive TUI, cwd mounted as the workspace
../codex/run.sh exec "..."  # non-interactive execution
../codex/run.sh shell       # bash via docker exec into the running container,
                            # or a fresh container when none is running
```

The container runs as user `zach` (uid 1000) with host networking; files it
creates in the mounted workspace are owned by uid 1000.

## Notes

- Mounts `~/.codex` and the current directory into the container.
- `run.sh` loads `~/.codex/.env`; it must export `OPENAI_API_KEY`.
- On hosts where the user's uid differs from 1000, files created inside the
  workspace will be owned by 1000.
- To share git identity with the container, extend `run.sh` with
  `-v $HOME/.gitconfig:/home/zach/.gitconfig`.
