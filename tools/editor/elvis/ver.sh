#!/bin/sh

h=${1:-"ver.h"}

# Helper function
available() { command -v "${1:?}" >/dev/null; }

echo "#define DATE \"`date +%Y-%m-%d`\"" >$h
if available git ; then
  git rev-parse HEAD | sed -n 's/^\(.......\).*/#define REV "git-\1"/p' >>$h
else
  echo "#define REV \"unknown\"" >>$h
fi
