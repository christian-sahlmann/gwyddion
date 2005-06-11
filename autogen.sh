#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# Tweaked by David Necas (Yeti) <yeti@gwyddion.net> from various other
# autogen.sh's.  This file is in the public domain.

DIE=0

PROJECT=Gwyddion
ACLOCAL_FLAGS="-I m4"
# When runnig autogen.sh one normally wants this.
CONF_FLAGS="--enable-maintainer-mode"
AUTOCONF=${AUTOCONF:-autoconf}
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}
AUTOMAKE=${AUTOMAKE:-automake}
ACLOCAL=${ACLOCAL:-aclocal}
AUTOHEADER=${AUTOHEADER:-autoheader}

echo "$*" | grep --quiet -- '--quiet\>\|--silent\>' && QUIET=">/dev/null"

($AUTOCONF --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**ERROR**: You must have \`autoconf' installed to re-generate"
  echo "all the $PROJECT Makefiles."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/autoconf/."
  DIE=1
  NO_AUTOCONF=yes
}

(grep "^AM_PROG_LIBTOOL" ./configure.ac >/dev/null) && {
  ($LIBTOOLIZE --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`libtool' installed."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/libtool/."
    DIE=1
    NO_LIBTOOL=yes
  }
}

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**ERROR**: You must have \`automake' installed to re-generate"
  echo "all the $PROJECT Makefiles."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/automake/."
  DIE=1
  NO_AUTOMAKE=yes
}

# The world is cruel.
if test -z "$NO_AUTOCONF"; then
  AC_VERSION=`$AUTOCONF --version | sed -e '2,$ d' -e 's/ *([^()]*)$//' -e 's/.* \(.*\)/\1/' -e 's/-p[0-9]\+//'`
  if test "$AC_VERSION" '<' "2.59"; then
    echo
    echo "**ERROR**: You need at least autoconf-2.59 installed to re-generate"
    echo "all the $PROJECT Makefiles."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/autoconf/."
    DIE=1
  else
    test -z "$QUIET" && echo "Autoconf $AC_VERSION: OK"
  fi
fi

if test -z "$NO_AUTOMAKE"; then
  AM_VERSION=`$AUTOMAKE --version | sed -e '2,$ d' -e 's/ *([^()]*)$//' -e 's/.* \(.*\)/\1/' -e 's/-p[0-9]\+//'`
  if test "$AM_VERSION" '<' "1.7"; then
    echo
    echo "**ERROR**: You need at least automake-1.6 installed to re-generate"
    echo "all the $PROJECT Makefiles."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/automake/."
    DIE=1
  else
    test -z "$QUIET" && echo "Automake $AM_VERSION: OK"
  fi
fi

# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || ($ACLOCAL --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**ERROR**: Missing \`aclocal'.  The version of \`automake'"
  echo "installed doesn't appear recent enough."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/automake/."
  DIE=1
}

if test -z "$NO_LIBTOOL"; then
  LT_VERSION=`$LIBTOOLIZE --version | grep libtool | sed 's/.* \([0-9.]*\)[-a-z0-9]*$/\1/'`
  if test "$LT_VERSION" '<' "1.4"; then
    echo
    echo "**ERROR**: You need at least libtool-1.4 installed to re-generate"
    echo "all the $PROJECT Makefiles."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/libtool/."
    DIE=1
  else
    test -z "$QUIET" && echo "Libtool $LT_VERSION: OK"
  fi
fi

if test "$DIE" -eq 1; then
  exit 1
fi

case $CC in
*xlc | *xlc\ * | *lcc | *lcc\ * )
  am_opt=--include-deps;;
esac

sh utils/update-potfiles.sh

dir=.
test -z "$QUIET" && echo processing $dir
(cd $dir && \
  eval $QUIET $LIBTOOLIZE --force && \
  eval $QUIET $ACLOCAL $ACLOCAL_FLAGS && \
  eval $QUIET $AUTOHEADER && \
  eval $QUIET $AUTOMAKE --add-missing $am_opt && \
  eval $QUIET $AUTOCONF) || {
    echo "**ERROR**: Re-generating failed.  You are allowed to shoot $PROJECT maintainer."
    echo "(BTW, why are you re-generating everything?)"
    exit 1
  }

if test -z "$*"; then
  echo "**Warning**: I am going to run \`configure' with $CONF_FLAGS."
  echo "If you wish to pass others to it, please specify them on the"
  echo "\`$0' command line (the defaults won't be used then)."
  echo
fi
./configure $CONF_FLAGS "$@"
