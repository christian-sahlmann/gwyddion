#!/bin/bash
function check() {
  if ! which $1 >/dev/null 2>&1; then
    DIE=1
    echo "$1 not found" 1>&2
  fi
}

DIE=
check man2html
check tidy
check sed
test -z "$DIE" || exit 1

f="$1"
name=$(basename "$f")
out=$name.html
sect=${name%*.}
name=${name%.[0-9]}
man2html -r "$f" \
  | sed 1,2d \
  | tidy -quiet -utf8 -asxhtml -wrap 0 >"$out.tmp"

date=$(sed -n 's/^Time: \(.*\)/\1/;t;d' "$out.tmp")
author=man2html
revision=generated

cat <<EOF
<?php
\$title = 'Gwyddion - $name($sect)';
\$fid = array('\$Revision: $revision \$', '\$Author: $author \$', '\$Date: $date $');
ini_set('include_path', '.:' . ini_get('include_path'));
include('_head.php');
include('_top.php');
?>
EOF

sed '1,/<body>/d; /^This document was created by/,$d' "$out.tmp" \
  | sed '$d; s#<a href="\.\./man1.*">\(.*\)</a>#\1#'

cat <<EOF

<?php include('_bot.php'); ?>
EOF
