#!/bin/bash

depend() {
  sudo apt install -y libpoppler-glib-dev
  echo "Install depend done"
}

build() {
  make
  sudo make install
  echo "Build done"
}

clean() {
  make clean
  echo "Clean done"
}

case $1 in
  dep )
    depend
    ;;
  build|-b )
    build
    ;;
  clean|-c )
    clean
    ;;
  * )
    echo "$(basename $0) {dep|build|-b|clean|-c}"
    ;;
esac
