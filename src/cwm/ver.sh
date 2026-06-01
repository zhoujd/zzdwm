#!/bin/sh

h=${1:-"ver.h"}

echo "#define DATE \"`date +%Y-%m-%d`\"" >$h
if command -v git >/dev/null; then
  git rev-parse HEAD | sed -n 's/^\(.......\).*/#define REV "git-\1"/p' >>$h
else
  echo "#define REV \"unknown\"" >>$h
fi
