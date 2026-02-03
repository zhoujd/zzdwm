#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT/.. && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

BIN=$CORE_ROOT/libexec/utils/dropbearmulti
PORT=${PORT:-2222}

TARGET=~/.dropbear
KEY=$TARGET/dropbear_ed25519_host_key

rm -rf $TARGET
mkdir -p $TARGET

OPT=(
    -p $PORT
    -r $KEY
)

$BIN dropbearkey -t ed25519 -f $KEY
$BIN dropbear ${OPT[@]}

echo "Run ssh server success."
