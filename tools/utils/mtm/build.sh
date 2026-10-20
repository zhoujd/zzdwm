#!/bin/bash

echo "Build full static binary"
make -C tools/mtm LDFLAGS=-static HEADERS='-DNCURSESW_INCLUDE_H="<ncurses.h>"'
