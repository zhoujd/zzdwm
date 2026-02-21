#!/bin/sh

file=${1:-"version.h"}

# Helper function
available() { command -v "${1:?}" >/dev/null; }

echo "#define DATE \"`date +%Y-%m-%d`\"" >$file
if available git ; then
  git rev-parse HEAD  | sed -n 's/^\(.......\).*/#define REV "git-\1"/p' >>$file
else
  echo "#define REV \"unknown\"" >>$file
fi
