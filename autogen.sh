#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# Tweaked by David Necas (Yeti) <yeti@gwyddion.net> from various other
# autogen.sh's.  This file is in the public domain.

DIE=0

PROJECT=Gwyddion
# When runnig autogen.sh one normally wants this.
CONF_FLAGS="--enable-maintainer-mode --enable-gtk-doc"
AUTOCONF=${AUTOCONF:-autoconf}
AUTOM4TE=${AUTOM4TE:-autom4te}
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}
AUTOMAKE=${AUTOMAKE:-automake}
ACLOCAL=${ACLOCAL:-aclocal}
AUTOHEADER=${AUTOHEADER:-autoheader}
GETTEXT=${GETTEXT:-gettext}
gettextize=`which gettextize`
GETTEXTIZE=${GETTEXTIZE:-$gettextize}

get_version() {
  local v
  local v2
  v=`$1 --version </dev/null 2>&1 | sed -e '2,$ d' -e 's/ *([^()]*)$//' -e 's/.* \(.*\)/\1/' -e 's/-p[0-9]*//'`
  v2=${v#*.}
  echo ${v%%.*}.${v2%%.*}
}

check_tool() {
  local name; name=$1
  local cmd; cmd="$2"
  local othercmds; othercmds="$3"
  local reqmajor; reqmajor=$4
  local reqminor; reqminor=$5
  local url; url="$6"
  local diewhy; diewhy=

  eval $VERBOSE echo "Looking for $cmd"
  if $cmd --version </dev/null >/dev/null 2>&1; then
    ver=`get_version "$cmd"`
    eval $VERBOSE echo "Found $cmd $ver"
    vermajor=${ver%%.*}
    verminor=${ver##*.}
    if test "$vermajor" -lt $reqmajor \
       || test "$vermajor" = $reqmajor -a "$verminor" -lt $reqminor; then
      diewhy=version
    else
      for othercmd in $othercmds; do
        eval $VERBOSE echo "Looking for $othercmd"
        if $othercmd --version </dev/null >/dev/null 2>&1; then
          otherver=`get_version "$othercmd"`
          eval $VERBOSE echo "Found $othercmd $otherver"
          if test "$otherver" != "$ver"; then
            diewhy=otherversion
            break
          else
            :
          fi
        else
          diewhy=othercmd
          break
        fi
      done
    fi
  else
    diewhy=cmd
  fi

  if test -n "$diewhy"; then
    echo "ERROR: $name at least $reqmajor.$reqminor is required to bootstrap $PROJECT."
    case $diewhy in
      version) echo "       You have only version $ver of $name installed.";;
      othercmd) echo "       It should also install command \`$othercmd' which is missing.";;
      otherversion) echo "       The version of \`$othercmd' differs from $cmd: $otherver != $ver.";;
      cmd) ;;
      *) echo "       *** If you see this, shoot the $PROJECT maintainer! ***";;
    esac
    echo "       Install the appropriate package for your operating system,"
    echo "       or get the source tarball of $name at"
    echo "       $url"
    echo
    DIE=1
  else
    eval $QUIET echo "$name $ver: OK"
  fi
}

echo "$*" | grep --quiet -- '--quiet\>\|--silent\>' && QUIET=">/dev/null"
echo "$*" | grep --quiet -- '--verbose\>\|--debug\>' || VERBOSE=">/dev/null"

check_tool Autoconf "$AUTOCONF" "$AUTOHEADER $AUTOM4TE" 2 60 ftp://ftp.gnu.org/pub/gnu/autoconf/
check_tool Automake "$AUTOMAKE" "$ACLOCAL" 1 11 ftp://ftp.gnu.org/pub/gnu/automake/
check_tool Libtool "$LIBTOOLIZE" "" 1 4 ftp://ftp.gnu.org/pub/gnu/libtool/
check_tool Gettext "$GETTEXT" "$GETTEXTIZE" 0 12 ftp://ftp.gnu.org/pub/gnu/gettext/

if test "$DIE" = 1; then
  exit 1
fi

case $CC in
*xlc | *xlc\ * | *lcc | *lcc\ * )
  am_opt=--include-deps;;
esac

# We don't want gettextize to mess with our files, but we want its config.rpath
if test -f config.rpath; then
  # Nothing to do
  :
else
  for x in prefix datarootdir gettext_dir; do
    eval `grep "^$x=" $GETTEXTIZE`
  done
  if cp "$gettext_dir/config.rpath" config.rpath; then
    # OK
    :
  else
    echo
    echo "ERROR: Cannot find config.rpath in $gettext_dir."
    echo "       Make sure you Gettext installation is complete."
    DIE=1
  fi
fi

if test "$DIE" = 1; then
  exit 1
fi

sh utils/update-potfiles.sh

(eval $QUIET $LIBTOOLIZE --automake --force \
  && eval $QUIET $ACLOCAL -I m4 $ACLOCAL_FLAGS \
  && eval $QUIET $AUTOHEADER \
  && eval $QUIET $AUTOMAKE --add-missing $am_opt \
  && eval $QUIET $AUTOCONF) \
  || {
    echo "ERROR: Re-generating failed."
    echo "       See above errors and complain to $PROJECT maintainer."
    exit 1
  }

if test -z "$*"; then
  echo "Note: I am going to run ./configure with the following flags:"
  echo "      $CONF_FLAGS"
  echo "      If you wish to pass others to it, specify them on the command line."
  echo
fi

./configure $CONF_FLAGS "$@"
