#!/bin/bash
if test "x$1" = "x-f"; then
  force=1
fi
libraries=$(sed -n '/^lib_LTLIBRARIES *= *\\/,/[^\\]$/H;/^lib_LTLIBRARIES *= *[a-zA-Z0-9]/H;${g;s/\\[[:space:]]*//g;s/.*= *//;p}' Makefile.am)
for l in $libraries; do
  l=${l%.la}
  target=$l.def
  test -f $target && mv -f $target $target~
  echo $target
  echo EXPORTS >$target
  nm .libs/$l.so | grep ' T ' | cut -d' ' -f3 | grep -v '^_' | sed -e 's/^/\t/' >>$target
  test -f $target~ && diff $target~ $target
done
