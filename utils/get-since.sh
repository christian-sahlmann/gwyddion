find . -name \*.c \
  | xargs sed -n 's/^ \* \([a-zA-Z0-9_]\+\): *$/\1/;t1;s/^ \* Since: \([0-9.]\+\).\? *$/\1/;t2;b;:1;h;b;:2;G;s/\n/ /;p' \
  | sort -g

