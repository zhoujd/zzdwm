#!/bin/sh

set -eu

output=${1:-version.h}
build_date=$(date +%Y-%m-%d)
build_revision=$(git rev-parse --short=7 HEAD 2>/dev/null) || build_revision=unknown
temporary="$output.tmp"

cat >"$temporary" <<EOF
#define BUILD_DATE "$build_date"
#define BUILD_REVISION "git-$build_revision"
EOF

if [ ! -f "$output" ] || ! cmp -s "$temporary" "$output"; then
    mv "$temporary" "$output"
else
    rm "$temporary"
fi
