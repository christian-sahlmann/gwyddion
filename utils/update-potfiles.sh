#!/bin/sh
exec >'po/POTFILES.in'

# Keep the `GENERATED' string quoted to prevent match here
echo '# List of source files containing translatable strings.'
echo '# This is a 'GENERATED' file, by utils/update-potfiles.sh.'

for dir in libgwyddion libprocess libdraw libgwydgets libgwymodule app modules; do
  echo
  echo "# $dir"
  find $dir -name \*.\[ch\] | xargs grep -E -l '\<N?_\(|\<gwy_sgettext\(' \
    | grep -v '/\(main\|test\)\.c' | sort
done
