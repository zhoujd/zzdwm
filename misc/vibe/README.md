README

## Install

```
./install.sh dep
./install.sh install codex
./install.sh install claude
```

Claude installs via npm into `~/.local` (the native installer at
claude.ai/install.sh is region-blocked on some networks). Node >= 18 and npm
are required; if the system node is older (Ubuntu 22.04 apt ships 12.x), a
user-local Node 22 tarball is installed into `~/.local` first. The native
installer is used only when npm is absent.

## Configure

```
# Set OPENAI_API_KEY in misc/vibe/codex/.env.sh.
./misc/vibe/install.sh config codex
./misc/vibe/install.sh config claude
```

Pass `all` instead of `codex` or `claude` to run a command for both tools.

The codex `.env` is intentionally untracked; create it from `.env.example`.
The tracked files — codex `config.toml` and `rules/default.rules`, claude
`settings.json` and `.env.sh` — can have their history reviewed and restored
with Git.

## Recover

```
./misc/vibe/install.sh recover codex
./misc/vibe/install.sh recover claude
```

This restores the tracked config files, creates backups of any different local
files, checks that the sources contain no NUL bytes, and verifies the tool
binary. It does not automatically start a session. For claude it also patches
`~/.claude.json` (onboarding complete, API key approved) — first-run onboarding
otherwise preflights api.anthropic.com and exits on region-blocked networks.

## Run

```
$ source misc/vibe/codex/.env.sh
$ codex

$ claude
```

Claude needs no sourcing: `settings.json` carries the API key in its `env`
block, and `.env.sh` remains for shells that want the key exported.
