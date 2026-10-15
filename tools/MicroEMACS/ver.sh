#!/bin/sh

h=${1:-"ver.h"}

echo "#define DATE \"`date +%Y-%m-%d`\"" >$h
revision=$(git rev-parse HEAD 2>/dev/null) || revision=
if [ -n "$revision" ]; then
  printf '%s\n' "$revision" | \
    sed -n 's/^\(.......\).*/#define REV "git-\1"/p' >>$h
else
  echo "#define REV \"unknown\"" >>$h
fi
