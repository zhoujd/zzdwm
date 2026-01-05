#!/bin/sh

# Helper function.
available() { command -v "${1:?}" >/dev/null; }

echo "#define DATE \"`date +%Y-%m-%d`\"" >rev.h
if available git ; then
  git rev-parse HEAD  | sed -n 's/^\(.......\).*/#define REV "git-\1"/p' >>rev.h
else
  echo "#define REV \"unknown\"" >>rev.h
fi
