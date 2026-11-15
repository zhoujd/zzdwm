#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

[ -f /etc/os-release ] && . /etc/os-release

dep() {
    local packages=(
        bubblewrap
        ca-certificates
        curl
        file
        git
        jq
        ripgrep
        tar
        unzip
        wget
    )

    case $ID in
        ubuntu|debian )
            sudo apt update
            sudo apt install -y "${packages[@]}"
            ;;
        bclinux|centos )
            sudo yum install -y "${packages[@]}"
            ;;
        * )
            echo "Unknown $ID"
            ;;
    esac
    echo "Install dependence done"
}

config_codex() {
    echo "Install codex configure files"
    mkdir -p ~/.codex
    mkdir -p ~/.codex/rules
    cp -rv "$SCRIPT_ROOT/codex/config.toml" ~/.codex/
    cp -rv "$SCRIPT_ROOT/codex/rules/default.rules" ~/.codex/rules/
    if [ ! -f "$HOME/.codex/.env" ]; then
        echo "Missing ~/.codex/.env; create it from misc/vibe/codex/.env" >&2
        return 1
    fi
    echo "Config codex done"
}

patch_claude_state() {
    ## First-run onboarding preflights api.anthropic.com and exits when it is
    ## unreachable (region-blocked networks serve 403), so a fresh install can
    ## never finish onboarding. Mark onboarding done and pre-approve the custom
    ## API key, keyed on the last 20 chars of the key.
    local key fingerprint
    key=$(sed -n 's/^export ANTHROPIC_API_KEY=//p' "$HOME/.claude/.env")
    if [ -z "$key" ]; then
        echo "No ANTHROPIC_API_KEY in $HOME/.claude/.env; skip state patch" >&2
        return 1
    fi
    fingerprint=${key: -20}
    python3 - "$fingerprint" <<'PYEOF'
import json, os, sys

fingerprint = sys.argv[1]
path = os.path.expanduser("~/.claude.json")
try:
    state = json.load(open(path))
except (FileNotFoundError, json.JSONDecodeError):
    state = {}
state["hasCompletedOnboarding"] = True
approved = state.get("customApiKeyResponses", {}).get("approved", [])
if fingerprint not in approved:
    approved.append(fingerprint)
state["customApiKeyResponses"] = {"approved": approved, "rejected": []}
json.dump(state, open(path, "w"), indent=2)
print(f"Patched {path}: onboarding complete, key approved")
PYEOF
}

config_claude() {
    echo "Install claude configure files"
    mkdir -p ~/.claude
    cp -rv "$SCRIPT_ROOT/claude/settings.json" ~/.claude/
    cp -rv "$SCRIPT_ROOT/claude/.env" ~/.claude/
    patch_claude_state
    echo "Config claude done"
}

recover_file() {
    local source=$1
    local destination=$2
    local timestamp=$3
    local temporary

    if [ ! -f "$source" ]; then
        echo "Missing recovery source: $source" >&2
        return 1
    fi

    if ! LC_ALL=C tr -d '\000' < "$source" | cmp -s - "$source"; then
        echo "Recovery source contains NUL bytes: $source" >&2
        return 1
    fi

    if [ -f "$destination" ] && ! cmp -s "$source" "$destination"; then
        cp -p "$destination" "${destination}.recovery-backup-${timestamp}"
        echo "Backed up existing file to ${destination}.recovery-backup-${timestamp}"
    fi

    temporary="${destination}.recovery-new-$$"
    cp "$source" "$temporary" || return 1
    mv "$temporary" "$destination"
    echo "Restored $destination"
}

recover_codex() {
    local timestamp

    timestamp=$(date +%Y%m%d-%H%M%S)
    mkdir -p "$HOME/.codex/rules"

    recover_file "$SCRIPT_ROOT/codex/config.toml" "$HOME/.codex/config.toml" "$timestamp" &&
        recover_file "$SCRIPT_ROOT/codex/rules/default.rules" "$HOME/.codex/rules/default.rules" "$timestamp" || return 1

    command -v codex >/dev/null || {
        echo "Codex binary is not installed" >&2
        return 1
    }
    codex --version
    echo "Recovery complete. Start Codex manually when you are ready."
}

recover_claude() {
    local timestamp

    timestamp=$(date +%Y%m%d-%H%M%S)
    mkdir -p "$HOME/.claude"

    recover_file "$SCRIPT_ROOT/claude/settings.json" "$HOME/.claude/settings.json" "$timestamp" &&
        recover_file "$SCRIPT_ROOT/claude/.env" "$HOME/.claude/.env" "$timestamp" || return 1
    patch_claude_state

    command -v claude >/dev/null || {
        echo "Claude Code binary is not installed" >&2
        return 1
    }
    claude --version
    echo "Recovery complete. Start Claude Code manually when you are ready."
}

ensure_node() {
    ## Claude Code and Codex need a modern node; distro node is often older
    ## (Ubuntu 22.04 apt ships 12.x). Install a user-local Node 22 tarball
    ## into ~/.local, which is also where their npm launchers go.
    local major
    major=$(node --version 2>/dev/null | sed 's/^v\([0-9]*\).*/\1/')
    if [ -n "$major" ] && [ "$major" -ge 18 ] && command -v npm >/dev/null 2>&1; then
        return 0
    fi
    if [ -x "$HOME/.local/bin/node" ]; then
        major=$("$HOME/.local/bin/node" --version | sed 's/^v\([0-9]*\).*/\1/')
        if [ -n "$major" ] && [ "$major" -ge 18 ]; then
            return 0
        fi
    fi

    echo "Install user-local Node 22 into $HOME/.local"
    local arch tarball tmp
    arch=$(uname -m | sed 's/^x86_64$/x64/;s/^aarch64$/arm64/')
    tarball=$(curl -fsSL https://nodejs.org/dist/latest-v22.x/ \
        | grep -o "node-v22[0-9.]*-linux-${arch}.tar.xz" | head -1)
    if [ -z "$tarball" ]; then
        echo "Cannot resolve a Node 22 tarball for ${arch}" >&2
        return 1
    fi
    mkdir -p "$HOME/.local"
    tmp=$(mktemp -d)
    ( cd "$tmp" && curl -fsSL -O "https://nodejs.org/dist/latest-v22.x/$tarball" \
        && tar -xJf "$tarball" -C "$HOME/.local" --strip-components=1 ) || { rm -rf "$tmp"; return 1; }
    rm -rf "$tmp"
    "$HOME/.local/bin/node" --version
}

latest_codex_tag() {
    curl -fsSL https://api.github.com/repos/openai/codex/releases/latest | \
        python3 -c 'import json, sys; print(json.load(sys.stdin)["tag_name"])' | \
        sed 's/^rust-//'
}

install_codex() {
    ensure_node || return 1
    export PATH="$HOME/.local/bin:$PATH"
    if command -v npm >/dev/null 2>&1; then
        local prefix
        prefix=$(npm config get prefix)
        if [ ! -w "$prefix" ]; then
            npm config set prefix "$HOME/.local"
            echo "Set npm prefix to $HOME/.local (was $prefix, not writable)"
        fi
        echo "Install codex via npm"
        npm install -g @openai/codex
        "$HOME/.local/bin/codex" --version
    else
        echo "npm not found, fall back to the GitHub binary"
        local DEST=~/Downloads
        local VER=$(latest_codex_tag)
        local PKG=codex-x86_64-unknown-linux-musl.tar.gz
        if [ -z "$VER" ]; then
            echo "Cannot detect the latest Codex release from GitHub" >&2
            return 1
        fi
        echo "Latest Codex release is ${VER}"
        mkdir -p $DEST
        pushd $DEST
        if [ ! -f codex-x86_64-unknown-linux-musl.tar.gz ]; then
            echo "Download codex binary"
            wget https://github.com/openai/codex/releases/download/rust-${VER}/${PKG}
        fi
        tar xfv codex-x86_64-unknown-linux-musl.tar.gz
        sudo cp -vf codex-x86_64-unknown-linux-musl /usr/local/bin/codex
        codex -V
        popd
    fi
    echo "Install codex done"
}

install_claude() {
    ## The native installer (claude.ai/install.sh) is unreachable from some
    ## networks (it redirects to a region-block HTML page), so install via npm
    ## when possible and keep the installer only as a fallback.
    ensure_node || return 1
    ## npm's shebang resolves `node` from PATH; make sure a ~/.local node
    ## (just installed or pre-existing) wins over an old /usr/bin one.
    export PATH="$HOME/.local/bin:$PATH"
    if command -v npm >/dev/null 2>&1; then
        local prefix
        prefix=$(npm config get prefix)
        if [ ! -w "$prefix" ]; then
            npm config set prefix "$HOME/.local"
            echo "Set npm prefix to $HOME/.local (was $prefix, not writable)"
        fi
        echo "Install claude-code via npm"
        npm install -g @anthropic-ai/claude-code
    else
        echo "npm not found, fall back to the native installer"
        curl -fsSL https://claude.ai/install.sh | bash -s stable
    fi
    "$HOME/.local/bin/claude" --version
    echo "Install claude done"
}

sudoers() {
    local RULE_FILE=/etc/sudoers.d/zach
    local USER_NAME=${SUDO_USER:-$(id -un)}
    local INSMOD=$(command -v insmod || echo /usr/sbin/insmod)
    local RMMOD=$(command -v rmmod || echo /usr/sbin/rmmod)
    local TMP_RULE=$(mktemp)

    echo "Grant sudo without password on insmod/rmmod for yzgpu load/unload"
    printf '%s ALL=(root) NOPASSWD: %s, %s\n' "$USER_NAME" "$INSMOD" "$RMMOD" > "$TMP_RULE"

    if ! visudo -cf "$TMP_RULE"; then
        echo "Invalid sudoers rule, abort"
        rm -f "$TMP_RULE"
        return 1
    fi

    sudo install -m 0440 "$TMP_RULE" "$RULE_FILE"
    rm -f "$TMP_RULE"
    echo "Install sudoers done"
}

dispatch() {
    local action=$1
    local tool=${2:-codex}

    case $tool in
        codex )
            "${action}_codex"
            ;;
        claude )
            "${action}_claude"
            ;;
        all )
            "${action}_codex" && "${action}_claude"
            ;;
        * )
            echo "Unknown tool: $tool" >&2
            ;;
    esac
}

case $1 in
    dep)
        dep
        ;;
    config|-c )
        dispatch config "$2"
        ;;
    recover|-r )
        dispatch recover "$2"
        ;;
    install|-i )
        dispatch install "$2"
        ;;
    sudo|-s )
        sudoers
        ;;
    * )
        echo "Usage $(basename $0) {dep|config|-c|recover|-r|install|-i|sudo|-s} [codex|claude|all]"
        ;;
esac
