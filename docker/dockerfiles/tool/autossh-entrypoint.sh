#!/bin/sh
set -eu

: "${AUTOSSH_M:=0}"
: "${SSH_MODE:=-R}"
: "${SSH_BIND_ADDRESS:=0.0.0.0}"
: "${SSH_TARGET_HOST:=localhost}"
: "${SSH_TARGET_PORT:=22}"
: "${SSH_HOST_USER:?SSH_HOST_USER is required}"
: "${SSH_HOST_NAME:?SSH_HOST_NAME is required}"
: "${SSH_OPTIONS:=}"

set -- ${SSH_MODE} "${SSH_BIND_ADDRESS}:${SSH_TARGET_HOST}:${SSH_TARGET_PORT}" \
    -N "${SSH_HOST_USER}@${SSH_HOST_NAME}" ${SSH_OPTIONS}

exec autossh -M "$AUTOSSH_M" "$@"
