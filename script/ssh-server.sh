#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT/.. && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

BIN=$CORE_ROOT/libexec/utils/dropbearmulti
PORT=${PORT:-2222}
USER=zach

echo "Setup $USER"
echo -n "$USER:123456" | sudo chpasswd

TARGET=~/.dropbear
rm -rf $TARGET
mkdir -p $TARGET

KEY=$TARGET/dropbear_ed25519_host_key
OPT=(
    -B
)

$BIN dropbearkey -t ed25519 -f $KEY
$BIN dropbear ${OPT[@]} -p $PORT -r $KEY

echo "Run ssh server success."
