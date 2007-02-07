#!/bin/sh
for x in */*.c; do
  echo "======= $x ======"
  sed -n '/^gwy_[a-z0-9_]*\(destroy\|finalize\)(/,/^}/p' $x
done
