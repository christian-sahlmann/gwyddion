#!/bin/sh
# @(#) $Id$
# An extremely simple Gwyddion plug-in example in shell.  Demonstrates data
# and metadata not outputted are retained from the original data.
# Written by Yeti <yeti@physics.muni.cz>.
# Public domain.
case "$1" in
    register)
    # Plug-in info.
    echo "yellow"
    echo "/_Test/Set Palette to _Yellow (shell)"
    echo "noninteractive with_defaults"
    ;;

    run)
    # We don't need to read the input when the output doesn't depend on it.
    echo "/0/base/palette=Yellow"
    ;;

    *)
    echo "*** Error: plug-in has to be called from Gwyddion plugin-proxy." 1>&2
    exit 1
    ;;
esac
