# @(#) $Id$

dnl Process this file with autoconf to produce a configure script.
AC_INIT(Gwyddion, 1.0, [yeti@gwyddion.net, klapetek@gwyddion.net])
PACKAGE_URL="http://gwyddion.net/"
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(app/app.c)
AC_CONFIG_FILES(\
  Makefile \
  app/Makefile \
  libdraw/Makefile \
  libgwyddion/Makefile \
  libgwydgets/Makefile \
  libgwymodule/Makefile \
  libprocess/Makefile \
  modules/Makefile \
  modules/file/Makefile \
  modules/process/Makefile \
  modules/graph/Makefile \
  modules/tools/Makefile \
  modules/layer/Makefile \
  plugins/Makefile \
  plugins/file/Makefile \
  plugins/process/Makefile \
  pixmaps/Makefile \
  devel-docs/Makefile \
  devel-docs/libgwyapp/Makefile \
  devel-docs/libgwyddion/Makefile \
  devel-docs/libgwydgets/Makefile \
  devel-docs/libgwydraw/Makefile \
  devel-docs/libgwymodule/Makefile \
  devel-docs/libgwyprocess/Makefile \
  gwyddion.spec \
  gwyddion.iss \
  gwyddion-devel.iss \
  gwyddion.pc \
  version.h \
)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([1.6 gnits check-news dist-bzip2])
AM_ACLOCAL_INCLUDE(m4)
AM_MAINTAINER_MODE

dnl Checks for programs.
dnl FIXME: the tagnames= inhibits at least part of the Fortran and C++ brain
dnl damage.
tagnames=
AC_DISABLE_STATIC
AC_PROG_AWK
AC_PROG_CC
AC_AIX
AC_GNU_SOURCE
AC_ISC_POSIX
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AM_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_CHECK_PROGS([SED],[sed])
AC_CHECK_PROGS([STRIP],[strip])
gtk_CHECK_GTK_DOC

dnl With GCC, be paranoiac.  It should compile really cleanly.
dnl Except -Wno-sign-compare, but older gcc's don't know it.
if test "$GCC" = yes; then
  WARNING_CFLAGS="-Wall -W -Wpointer-arith -Werror-implicit-function-declaration -Wno-system-headers -Wshadow -Wredundant-decls"
fi

case $target_os in
  mingw*)
    TARGET_CFLAGS="-mms-bitfields"
    TARGET_LDFLAGS="-mwindows"
    TARGET_TARGETS="README.wri COPYING.wri"
    ;;
  *)
    STRIP="echo"
    ;;
esac
TARGET_CFLAGS="$TARGET_CFLAGS -march=$target_cpu"

dnl Dirty path hack.  Helps some people with badly set up search paths.
if test "$prefix" = "NONE"; then
  LDFLAGS="$LDFLAGS -L$ac_default_prefix/lib"
  CPPFLAGS="$CPPFLAGS -I$ac_default_prefix/include"
else
  LDFLAGS="$LDFLAGS -L$prefix/lib"
  CPPFLAGS="$CPPFLAGS -I$prefix/include"
fi

PKG_CHECK_MODULES(PACKAGE, [gtk+-2.0 >= 2.2.0 libpng12])
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0])

AC_CHECK_LIB([m],[sqrt])
AC_CHECK_LIB([jpeg],[jpeg_abort])
AC_CHECK_LIB([tiff],[TIFFOpen])
PACKAGE_LIBS="$PACKAGE_LIBS $LIBS"

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS( \
  jpeglib.h \
  tiffio.h \
  time.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_STRUCT_TM
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL
#gwy_C_VARARG_MACROS

dnl Checks for library functions.
AC_CHECK_FUNCS([getline])
AC_CHECK_FUNCS([setenv])

dnl RPM
RPM_RELEASE=1
RPM_VERSION=`echo AC_PACKAGE_VERSION`

dnl What were you smoking?  Why EXEEXT contains the dot while OBJEXT doesn't?
#if test "x$OBJEXT" != "x"; then
#  OBJEXT=".$OBJEXT";
#fi

CFLAGS="$TARGET_CFLAGS $WARNING_CFLAGS $PACKAGE_CFLAGS $CFLAGS"
LDFLAGS="$TARGET_LDFLAGS $PACKAGE_LIBS $LDFLAGS"

AC_SUBST(RPM_RELEASE)
AC_SUBST(RPM_VERSION)
AC_SUBST(PACKAGE_URL)
AC_DEFINE_UNQUOTED(PACKAGE_URL,"$PACKAGE_URL",
          [Define to package home page, as a string.])
AC_SUBST(TARGET_TARGETS)

gwymoduledir=$libdir/gwyddion/modules
gwyplugindir=$libexecdir/gwyddion/plugins
gwypixmapdir=$datadir/gwyddion/pixmaps
AC_SUBST(gwymoduledir)
AC_SUBST(gwyplugindir)
AC_SUBST(gwypixmapdir)

AC_OUTPUT
