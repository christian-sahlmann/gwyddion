#!/bin/sh
# @(#) $Id$
# An extremely simple Gwyddion plug-in example in shell.  Demonstrates data
# and metadata not outputted are retained from the original data.
# Written by Yeti <yeti@physics.muni.cz>.
# Public domain.
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
