# @(#) $Id$

dnl Process this file with autoconf to produce a configure script.
AC_INIT(Gwyddion, 1.5cvs20041004, [yeti@gwyddion.net, klapetek@gwyddion.net])
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
  po/Makefile.in \
  perl/Makefile \
  python/Makefile \
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
AM_INIT_AUTOMAKE([1.6 gnu check-news dist-bzip2])
AM_ACLOCAL_INCLUDE(m4)
AM_MAINTAINER_MODE

dnl Checks for programs.
AC_DISABLE_STATIC
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_F77
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
AM_PATH_PYTHON(2.2,,:)
AC_PATH_PROGS(PERL,perl5 perl,:)
AC_PATH_PROGS(POD2MAN,pod2man,:)
gtk_CHECK_GTK_DOC
AC_PATH_PROGS(PASCAL,fpc,:)

# Check Pascal version
# The outer [] are necessary to quote the inner [] inside sed expressions
if test "$PASCAL" != :; then
  AC_MSG_CHECKING([if fpc version >= 1.9])
  fpc_version=`$PASCAL -iV`
  [fpc_major=`echo $fpc_version | sed -e 's/\..*//'`]
  [fpc_minor=`echo $fpc_version | sed -e 's/[^.]*\.//' -e 's/\..*//'`]
  if test $fpc_major -gt 1; then
    fpc_version_ok=yes
  else
    if test $fpc_major = 1 -a $fpc_minor -gt 8; then
      fpc_version_ok=yes
    else
      fpc_version_ok=no
      PASCAL=:
    fi
  fi
  AC_MSG_RESULT([$fpc_version_ok])
fi

AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])
AM_CONDITIONAL([HAVE_PERL], [test "$PERL" != :])
AM_CONDITIONAL([HAVE_PASCAL], [test "$PASCAL" != :])

if test "$PERL" != :; then
  eval `$PERL -V:installsitelib | sed -e 's/^/perl/'`
  eval `$PERL -V:installprefix | sed -e 's/^/perl/'`
  PERL_SITE_LIB=`echo $perlinstallsitelib | sed -e "s#^$perlinstallprefix##"`
fi

dnl With GCC, be paranoiac.  It should compile really cleanly.
if test "$GCC" = yes; then
  ye_PROG_CC_OPTION([WNO_EMPTY_FMT],[-Wno-format-zero-length],,)
  ye_PROG_CC_OPTION([WNO_SIGN_COMPARE],[-Wno-sign-compare],,)
  ye_PROG_CC_OPTION([WNO_SYS_HEADERS],[-Wno-system-headers],,)
  ye_PROG_CC_OPTION([ERR_IFD],[-Werror-implicit-function-declaration],,)
  ye_PROG_CC_OPTION([WREDUNDANT],[-Wredundant-decls],,)
  # FIXME: We would like an error, but gcc can't make one w/o -std, which is
  # too much.
  ye_PROG_CC_OPTION([WMIXED_DECL],[-Wdeclaration-after-statement],,)
  WARNING_CFLAGS="-Wall -W -Wshadow -Wpointer-arith $PROG_CC_ERR_IFD $PROG_CC_WNO_SYS_HEADERS $PROG_CC_WREDUNDANT $PROG_CC_WNO_EMPTY_FMT $PROG_CC_WNO_SIGN_COMPARE $PROG_CC_WMIXED_DECL"
fi

case $target_os in
  mingw*)
    TARGET_CFLAGS="-mms-bitfields"
    TARGET_LDFLAGS="-mwindows"
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

PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.2.0])
PKG_CHECK_MODULES(GOBJECT, [gobject-2.0])
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0])
PKG_CHECK_MODULES(GTKGLEXT, [gtkglext-1.0 pangoft2])

AC_CHECK_LIB([m],[sqrt])
AC_CHECK_LIB([tiff],[TIFFOpen])
PACKAGE_LIBS="$PACKAGE_LIBS $GOBJECT_LIBS $LIBS"
# XXX: need Gtk+ headers.
PACKAGE_CFLAGS="$PACKAGE_CFLAGS $GTK_CFLAGS $GTKGLEXT_CFLAGS"

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS( \
  GL/gl.h \
  tiffio.h \
  fcntl.h \
  stdbool.h \
  sys/stat.h \
  sys/types.h \
  unistd.h \
  time.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_STRUCT_TM
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL

dnl Checks for library functions.
AC_FUNC_MMAP
AC_CHECK_FUNCS([getline])
AC_CHECK_FUNCS([setenv])

dnl I18n stuff
GETTEXT_PACKAGE=gwyddion
AM_GLIB_GNU_GETTEXT
XGETTEXT="$XGETTEXT --from-code=UTF-8"
LIBS="$LIBS $INTLLIBS"
ALL_LINGUAS=
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])
AC_SUBST(GETTEXT_PACKAGE)

AM_GLIB_DEFINE_LOCALEDIR(GWY_LOCALEDIR)

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
AC_SUBST(PERL_SITE_LIB)
AC_SUBST(PACKAGE_URL)
AC_DEFINE_UNQUOTED(PACKAGE_URL,"$PACKAGE_URL",
          [Define to package home page, as a string.])

gwymoduledir=$libdir/gwyddion/modules
gwyplugindir=$libexecdir/gwyddion/plugins
gwypixmapdir=$datadir/gwyddion/pixmaps
AC_SUBST(gwymoduledir)
AC_SUBST(gwyplugindir)
AC_SUBST(gwypixmapdir)

AC_OUTPUT
