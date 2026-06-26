#!/bin/bash

depend() {
  sudo apt install -y libpoppler-glib-dev
}

build() {
  make
  sudo make install
}

case $1 in
  dep )
    depend
    ;;
  build|-b )
    build
    ;;
  * )
    echo $(basename $0) {dep}
    ;;
esac
