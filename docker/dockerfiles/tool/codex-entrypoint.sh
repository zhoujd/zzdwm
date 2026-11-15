#!/bin/bash

set -e

## The launcher may start the container with the host user's uid so files
## created in the mounted workspace belong to the host user; make $HOME
## writable by that uid before codex starts.
if [ "$(id -u)" != 0 ] && [ ! -w "$HOME" ]; then
    sudo chown -R "$(id -u):$(id -g)" "$HOME"
fi

exec "$@"
