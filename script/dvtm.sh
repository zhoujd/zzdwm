#!/bin/sh

name=z1
case $TERM in
    dvtm* )
        echo "dvtm $name exists."
        ;;
    * )
        abduco -A $name dvtm-status
        ;;
esac
