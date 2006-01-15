# @(#) $Id$
dnl Process this file with autoconf to produce a configure script.
AC_INIT(Gwyddion, 1.99.5cvs20060115, [http://trific.ath.cx/bugzilla/])
# Use -release for unstable version libs instead of -version-info, because
# we *do* break compatibility each version
LIBRARY_RELEASE=1.99.6
PACKAGE_URL="http://gwyddion.net/"
AC_PREREQ(2.59)
AC_CONFIG_SRCDIR(app/app.c)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_FILES(\
  Makefile \
  m4/Makefile \
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
  ruby/Makefile \
  data/Makefile \
  data/glmaterials/Makefile \
  data/gradients/Makefile \
  utils/Makefile \
  devel-docs/Makefile \
  devel-docs/libgwyapp/Makefile \
  devel-docs/libgwyddion/Makefile \
  devel-docs/libgwydgets/Makefile \
  devel-docs/libgwydraw/Makefile \
  devel-docs/libgwymodule/Makefile \
  devel-docs/libgwyprocess/Makefile \
  gwyddion.spec:data/gwyddion.spec.in \
  data/gwyddion.iss \
  data/gwyddion-devel.iss \
  data/gwyddion.pc \
  data/config.h.win32 \
)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_TARGET
dnl F***ing automake requires the silly ABOUT-NLS file.  Use `foreign'
dnl strictness level.
AM_INIT_AUTOMAKE([1.7 foreign check-news dist-bzip2])
AM_MAINTAINER_MODE
AC_SUBST(LIBRARY_RELEASE)

#############################################################################
# Checks for programs and systems.
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
AC_CHECK_PROGS([STRIP],[strip])
AC_CHECK_PROGS([XSLTPROC],[xsltproc],[:])
GTK_DOC_CHECK(1.4)

#############################################################################
# Target sytstem.
AC_MSG_CHECKING([for target architecture])
case x"$target" in
  xNONE | x)
    target_or_host="$host" ;;
  *)
    target_or_host="$target" ;;
esac
AC_MSG_RESULT([$target_or_host])

AC_MSG_CHECKING([for 32bit])
case "$target_or_host" in
  # Highly incomplete
  i@<:@3456@:>@86-*)
    arch_32bit=yes
    ;;
  *)
    # The trouble is, we cannot reliably detect things like CFLAGS=-m32
    arch_32bit="maybe not"
    ;;
esac
AC_MSG_RESULT([$arch_32bit])

#############################################################################
# Win32.
AC_MSG_CHECKING([for native Win32])
case "$target_or_host" in
  *-*-mingw*)
    os_win32=yes
    TARGET_CFLAGS="-mms-bitfields"
    TARGET_LDFLAGS="-mwindows"
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL([OS_WIN32],[test "$os_win32" = "yes"])

#############################################################################
# Module symbol resolution.
# We try not to link modules with libraries and let symbols resolve through
# app, because it means faster app startup and much faster compilation.  But
# on some systems that isn't possible.
AM_CONDITIONAL([MODULE_DEPENDENCIES],[test "x$allow_undefined_flag" = xunsupported])

#############################################################################
# XDG stuff
AC_CHECK_PROGS([UPDATE_DESKTOP_DATABASE],[update-desktop-database],:)
AC_CHECK_PROGS([UPDATE_MIME_DATABASE],[update-mime-database],:)
GWY_ENABLE([desktop-file-update],
           [desktop_file_update],
           [Update Freedesktop databases])
AM_CONDITIONAL([DESKTOP_FILE_UPDATE],[test "$enable_desktop_file_update" != no])

#############################################################################
# Python
GWY_ENABLE([python],,[Install Python modules and plug-ins])
AM_PATH_PYTHON(2.2,,:)
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != : -a "$enable_python" != no])

#############################################################################
# Check for Perl
GWY_ENABLE([perl],,[Install Perl modules and plug-ins])
AC_PATH_PROGS(PERL,perl5 perl,:)
if test "$PERL" != :; then
  AC_PATH_PROGS(POD2MAN,pod2man,:)
fi
AM_CONDITIONAL([HAVE_PERL], [test "$PERL" != : -a "$enable_perl" != no])

#############################################################################
# Check for Ruby
# The outer [] are necessary to quote the inner [] inside Ruby
GWY_ENABLE([ruby],,[Install Ruby modules and plug-ins])
AC_PATH_PROGS(RUBY,ruby,:)
if test "$RUBY" != :; then
  AC_MSG_CHECKING([if ruby version >= 1.8])
  if $RUBY -e ["
require 'rbconfig'
c = Config::CONFIG
if c['MAJOR'] > '1' || c['MAJOR'] == '1' && c['MINOR'] >= '8'
  exit 0
end
exit 1"]; then
    ruby_version_ok=yes
  else
    ruby_version_ok=no
    RUBY=:
  fi
  AC_MSG_RESULT([$ruby_version_ok])
fi
AM_CONDITIONAL([HAVE_RUBY], [test "$RUBY" != : -a "$enable_ruby" != no])

#############################################################################
# Check for Ruby narray module
ruby_has_narray=no
if test "$RUBY" != :; then
  AC_MSG_CHECKING([for ruby narray module])
  if $RUBY -rnarray -e 'exit 0' >/dev/null 2>&1; then
    ruby_has_narray=yes
  else
    ruby_has_narray=no
  fi
  AC_MSG_RESULT([$ruby_has_narray])
fi
AM_CONDITIONAL([HAVE_RUBY_NARRAY], [test "$ruby_has_narray" = yes])

#############################################################################
# Check for Pascal compiler
# The outer [] are necessary to quote the inner [] inside sed expressions
GWY_ENABLE([pascal],,[Build sample Pascal plug-ins])
AC_PATH_PROGS(PASCAL,fpc,:)
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
AM_CONDITIONAL([HAVE_PASCAL], [test "$PASCAL" != : -a "$enable_pascal" != no])

#############################################################################
# With GCC, be paranoiac.  It should compile really cleanly.
if test "$GCC" = yes; then
  GWY_PROG_CC_OPTION([WNO_EMPTY_FMT],[-Wno-format-zero-length],,)
  GWY_PROG_CC_OPTION([WNO_SYS_HEADERS],[-Wno-system-headers],,)
  GWY_PROG_CC_OPTION([WNO_POINTER_SIGN],[-Wno-pointer-sign],,)
  GWY_PROG_CC_OPTION([ERR_IFD],[-Werror-implicit-function-declaration],,)
  # FIXME: We would like an error, but gcc can't make one w/o -std, which is
  # too much.
  GWY_PROG_CC_OPTION([WMIXED_DECL],[-Wdeclaration-after-statement],,)
  GWY_PROG_CC_OPTION([FNO_TRAP_MATH],[-fno-trapping-math],,)
  GWY_PROG_CC_OPTION([FNO_MATH_ERRNO],[-fno-math-errno],,)
  # Flags we can't use:
  # -Wredundant-decls makes glib.h + gstdio.h noisy
  # -Wstrict-protoypes makes gtkitemfactory.h noisy, but they have a
  #    comment there they can't fix it due to compatibility
  # -Wwrite-strings because it makes all struct initializations very noisy
  WARNING_CFLAGS="-Wall -W -Wshadow -Wpointer-arith -Wno-sign-compare -Wundef $PROG_CC_ERR_IFD $PROG_CC_WNO_SYS_HEADERS $PROG_CC_WNO_POINTER_SIGN $PROG_CC_WNO_EMPTY_FMT $PROG_CC_WMIXED_DECL"
  MATH_CFLAGS="$PROG_CC_FNO_TRAP_MATH $PROG_CC_FNO_MATH_ERRNO"
fi

#############################################################################
# Dirty path hack.  Helps some people with badly set up search paths.
if test "$prefix" = "NONE"; then
  LDFLAGS="$LDFLAGS -L$ac_default_prefix/lib"
  CPPFLAGS="$CPPFLAGS -I$ac_default_prefix/include"
else
  LDFLAGS="$LDFLAGS -L$prefix/lib"
  CPPFLAGS="$CPPFLAGS -I$prefix/include"
fi

AC_PATH_X
test "x$x_includes" = "x" || CPPFLAGS="$CPPFLAGS -I$x_includes"
test "x$x_libraries" = "x" || LDFLAGS="$LDFLAGS -L$x_libraries"

PKG_CHECK_MODULES(GTK, [pangoft2 gtk+-2.0 >= 2.6.0])
PKG_CHECK_MODULES(GOBJECT, [gobject-2.0])
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0])

ORIG_LIBS="$LIBS"
AC_CHECK_LIB(m,sqrt,,[])

#############################################################################
# FFTW3
GWY_WITH([fftw3],,[FFTW3 library])
if test "x$enable_fftw3" != "xno"; then
  PKG_CHECK_MODULES(FFTW3, [fftw3],
    [AC_DEFINE(HAVE_FFTW3,1,[Define if we have the FFTW3 package.])],
    enable_fftw3=no)
fi
if test "x$enable_fftw3" != "xno"; then
  if test "x$arch_32bit" != "xyes"; then
    PKG_CHECK_MODULES(FFTW3_DUMMY,[fftw3 >= 3.1],[:],
      AC_MSG_WARN([FFTW3 older than 3.1 may cause crashes on 64bit systems.
                    Consider installation of FFTW3 at least 3.1.  Alternatively
                    you can disable FFTW3 with --without-fftw3.]))
  fi
  FFTW3_DEPENDENCY=fftw3
else
  FFTW3_DEPENDENCY=
fi
AC_SUBST([FFTW3_DEPENDENCY])

#############################################################################
# GtkGLExt
GWY_WITH([gl],,[OpenGL 3D view widgets])
if test "x$enable_gl" != "xno"; then
  # Repeat pangoft2 here to maintain assertion GTK flags are subset of GTKGLEXT
  PKG_CHECK_MODULES(GTKGLEXT,[pangoft2 gtkglext-1.0],
                    [AC_DEFINE(HAVE_GTKGLEXT,1,
                               [Define if we have the GtkGLExt package.])],
                    enable_gl=no)
fi
if test "x$enable_gl" != "xno"; then
  GTKGLEXT_DEPENDENCY=gtkglext-1.0
else
  GTKGLEXT_DEPENDENCY=
  GTKGLEXT_CFLAGS="$GTK_CFLAGS"
  GTKGLEXT_LIBS="$GTK_LIBS"
fi
AC_SUBST([GTKGLEXT_DEPENDENCY])

#############################################################################
# TIFF
# TODO: Add some message to end of configure what is supported and what isn't
# Missing libtiff is not fatal.
GWY_WITH([libtiff],,[build with TIFF support])
if test x$enable_libtiff != xno && test -z "$LIBTIFF"; then
  AC_CHECK_LIB(tiff, TIFFReadScanline,
    [AC_CHECK_HEADER(tiffio.h, LIBTIFF='-ltiff', [enable_libtiff=no])],
    [AC_CHECK_LIB(tiff, TIFFWriteScanline,
      [AC_CHECK_HEADER(tiffio.h, LIBTIFF='-ltiff -ljpeg -lz',
        [enable_libtiff=no])],
      [AC_CHECK_LIB(tiff34, TIFFFlushData,
        [AC_CHECK_HEADER(tiffio.h, LIBTIFF='-ltiff34 -ljpeg -lz',
          [enable_libtiff=no])],
        [enable_libtiff=no;],
        [-ljpeg -lz])],
      [-ljpeg -lz])],
    [])
fi
if test x$enable_libtiff != xno && test -n "$LIBTIFF"; then
  AC_DEFINE(HAVE_TIFF,1,[Define if we have the TIFF library.])
fi
AM_CONDITIONAL([HAVE_TIFF],[test x$enable_libtiff != xno && test -n "$LIBTIFF"])
AC_SUBST(LIBTIFF)

#############################################################################
# Check for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_MSG_CHECKING([for anyone actually reading this nonsense])
AC_MSG_RESULT([no])
AC_CHECK_HEADERS( \
  fcntl.h \
  stdbool.h \
  sys/stat.h \
  sys/types.h \
  unistd.h \
  time.h)

#############################################################################
# Check for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_STRUCT_TM
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL

#############################################################################
# Check for library functions.
AC_FUNC_MMAP
AC_CHECK_FUNC([cbrt], [gwy_have_cbrt=yes], [gwy_have_cbrt=no])
AC_CHECK_FUNC([hypot], [gwy_have_hypot=yes], [gwy_have_hypot=no])
AC_CHECK_FUNC([pow10], [gwy_have_pow10=yes], [gwy_have_pow10=no])
AC_CHECK_FUNC([acosh], [gwy_have_acosh=yes], [gwy_have_acosh=no])
AC_CHECK_FUNC([asinh], [gwy_have_asinh=yes], [gwy_have_asinh=no])
AC_CHECK_FUNC([atanh], [gwy_have_atanh=yes], [gwy_have_atanh=no])

#############################################################################
# I18n
GETTEXT_PACKAGE=$PACKAGE_TARNAME
AM_GNU_GETTEXT([external])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])
AC_SUBST(GETTEXT_PACKAGE)

#############################################################################
# Libs
BASIC_LIBS="$GOBJECT_LIBS $INTLLIBS $LIBS $LDFLAGS"
LIBS="$ORIG_LIBS"
AC_SUBST(BASIC_LIBS)
BASIC_CFLAGS="$GOBJECT_CFLAGS $GMODULE_CFLAGS $GTKGLEXT_CFLAGS $CFLAGS"

CFLAGS="$TARGET_CFLAGS $WARNING_CFLAGS $MATH_CFLAGS $BASIC_CFLAGS"
LDFLAGS="$TARGET_LDFLAGS"

#############################################################################
# RPM
RPM_RELEASE=1
RPM_VERSION=`echo AC_PACKAGE_VERSION`

AC_SUBST(RPM_RELEASE)
AC_SUBST(RPM_VERSION)
AC_SUBST(PACKAGE_URL)
AC_DEFINE_UNQUOTED(PACKAGE_URL,"$PACKAGE_URL",
                   [Define to package home page, as a string.])

#############################################################################
# Generate gwyconfig
#
# NB: When you add definitions here, add them to data/gwyconfig.h.win32 too.
# 
AC_CONFIG_COMMANDS([gwyconfig.h],
[
  outfile=gwyconfig.h-tmp
  cat > $outfile <<\_______EOF
/*
 * gwyconfig.h
 *
 * This is a generated file.  Please modify 'configure.ac'
 */

#ifndef __GWY_CONFIG_H__
#define __GWY_CONFIG_H__

_______EOF

  if test x$enable_gl = xno; then
    echo '#undef GWYDDION_HAS_OPENGL' >>$outfile
  else
    echo '#define GWYDDION_HAS_OPENGL 1' >>$outfile
  fi
  echo >>$outfile

  if test x$gwy_have_pow10 = xno; then
    echo '#define pow10(x) exp(G_LN10*(x))' >>$outfile
  else
    echo '/* pow10() is defined */' >>$outfile
  fi

  if test x$gwy_have_hypot = xno; then
    echo 'static inline double hypot(double x, double y) { return sqrt(x*x + y*y); }' >>$outfile
  else
    echo '/* hypot() is defined */' >>$outfile
  fi

  if test x$gwy_have_cbrt = xno; then
    echo '#define cbrt(x) pow((x), 1.0/3.0)' >>$outfile
  else
    echo '/* cbrt() is defined */' >>$outfile
  fi

  if test x$gwy_have_acosh = xno; then
    echo 'static inline double acosh(double x) { return log(x+sqrt(x*x - 1)); }' >>$outfile
  else
    echo '/* acosh() is defined */' >>$outfile
  fi

  if test x$gwy_have_asinh = xno; then
    echo 'static inline double asinh(double x) { return log(x+sqrt(x*x + 1)); }' >>$outfile
  else
    echo '/* asinh() is defined */' >>$outfile
  fi

  if test x$gwy_have_atanh = xno; then
    echo 'static inline double atanh(double x) { return log((1 + x)/(1 - x)); }' >>$outfile
  else
    echo '/* atanh() is defined */' >>$outfile
  fi

  cat >>$outfile <<_______EOF

#endif /* __GWY_CONFIG_H__ */
_______EOF


  if cmp -s $outfile gwyconfig.h; then
    AC_MSG_NOTICE([gwyconfig.h is unchanged])
    rm -f $outfile
  else
    mv $outfile gwyconfig.h
  fi
],
[
  enable_gl=$enable_gl
  gwy_have_pow10=$gwy_have_pow10
  gwy_have_hypot=$gwy_have_hypot
  gwy_have_cbrt=$gwy_have_cbrt
  gwy_have_acosh=$gwy_have_acosh
  gwy_have_asinh=$gwy_have_asinh
  gwy_have_atanh=$gwy_have_atanh
])

AC_OUTPUT

echo "================================================================="
echo "Configuration:"
echo "  TIFF support (including JPK files):  $enable_libtiff"
echo "  With FFTW3:                          $enable_fftw3"
echo "  With OpenGL 3D widgets:              $enable_gl"
echo "================================================================="

