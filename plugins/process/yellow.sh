#!/bin/bash
case "$1" in
    register)
    echo "yellow"
    echo "/Set Palette to _Yellow (shell :o)"
    echo "noninteractive with_defaults"
    ;;

    run)
    echo "/0/base/palette=Yellow"
    ;;

    *)
    echo "*** Error: plug-in has to be called from Gwyddion plugin-proxy." 1>&2
    exit 1
    ;;
esac
