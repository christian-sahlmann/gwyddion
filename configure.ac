# @(#) $Id$
dnl Process this file with autoconf to produce a configure script.
m4_define([gwy_version_major],[2])
m4_define([gwy_version_minor],[16])
m4_define([gwy_version_string],[gwy_version_major.gwy_version_minor])
AC_INIT([Gwyddion], [gwy_version_string], [klapetek@gwyddion.net])
# Use -release for unstable version libs instead of -version-info, because
# we *do* break compatibility each version
#LIBRARY_RELEASE=1.99.9
PACKAGE_URL="http://gwyddion.net/"
AC_PREREQ(2.60)
AC_CONFIG_SRCDIR(app/app.c)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_FILES(\
  Makefile \
  m4/Makefile \
  app/Makefile \
  libdraw/Makefile \
  libgwyddion/Makefile \
  libgwyddion/gwyversion.h \
  libgwydgets/Makefile \
  libgwymodule/Makefile \
  libprocess/Makefile \
  modules/Makefile \
  modules/file/Makefile \
  modules/graph/Makefile \
  modules/layer/Makefile \
  modules/process/Makefile \
  modules/pygwy/Makefile \
  modules/tools/Makefile \
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
  utils/gwywin32.bat \
  thumbnailer/Makefile \
  devel-docs/Makefile \
  devel-docs/libgwyapp/Makefile \
  devel-docs/libgwyapp/releaseinfo.xml \
  devel-docs/libgwyddion/Makefile \
  devel-docs/libgwyddion/releaseinfo.xml \
  devel-docs/libgwydgets/Makefile \
  devel-docs/libgwydgets/releaseinfo.xml \
  devel-docs/libgwydraw/Makefile \
  devel-docs/libgwydraw/releaseinfo.xml \
  devel-docs/libgwymodule/Makefile \
  devel-docs/libgwymodule/releaseinfo.xml \
  devel-docs/libgwyprocess/Makefile \
  devel-docs/libgwyprocess/releaseinfo.xml \
  gwyddion.spec:data/gwyddion.spec.in \
  data/gwyddion.iss \
  data/gwyddion-devel.iss \
  data/gwyddion.pc \
  data/config.h.win32 \
)
AC_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.7 gnu check-news dist-bzip2])
AM_MAINTAINER_MODE

GWY_VERSION_MAJOR=gwy_version_major
GWY_VERSION_MINOR=gwy_version_minor
GWY_VERSION_STRING='"gwy_version_string"'
AC_SUBST(GWY_VERSION_MAJOR)
AC_SUBST(GWY_VERSION_MINOR)
AC_SUBST(GWY_VERSION_STRING)
AC_SUBST(LIBRARY_RELEASE)

#############################################################################
# Checks for programs and systems.
# Make libtool stop checking for FORTRAN.
m4_defun([_LT_AC_LANG_F77_CONFIG],[:])
AC_DISABLE_STATIC
AC_PROG_CC
AC_PROG_CXX
dnl FIXME: Can't do this, see math section below
dnl AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_CHECK_PROGS([XSLTPROC],[xsltproc],[:])
AC_CHECK_PROGS([EPYDOC],[epydoc],[:])
GTK_DOC_CHECK(1.8)

#############################################################################
# 64bit (for FFTW 3.0 warning)
# Unfortunately, AC_CHECK_SIZEOF() is capable only of preprocessor symbol
# production while we need to know the size now.  We don't care much about
# the exact model: if pointers are larger than 32bit it can possibly break,
# and so we will warn.
AC_CHECK_SIZEOF([void*])
if test $ac_cv_sizeof_voidp = 4; then
  arch_32bit=yes
else
  arch_32bit=no
fi

#############################################################################
# Win32.
AC_MSG_CHECKING([for native Win32])
case "$host_os" in
  mingw*)
    os_win32=yes
    HOST_CFLAGS="-mms-bitfields"
    HOST_LDFLAGS="-mwindows"
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL([OS_WIN32],[test "$os_win32" = "yes"])

#############################################################################
# Darwin.
AC_MSG_CHECKING([for Darwin])
case "$host_os" in
  darwin*)
    os_darwin=yes
    ;;
  *)
    os_darwin=no
    ;;
esac
AC_MSG_RESULT([$os_darwin])
AM_CONDITIONAL([OS_DARWIN],[test "$os_darwin" = "yes"])

#############################################################################
# Module symbol resolution.
# We try not to link modules with libraries and let symbols resolve through
# app, because it means faster app startup and much faster compilation.  But
# on some systems that isn't possible.
AC_ARG_ENABLE(library-bloat,
              AC_HELP_STRING([--enable-library-bloat],
                             [Make linking very slow]),
                             [],
                             [enable_library_bloat=no])
if test "x$enable_library_bloat" != xyes \
   && test "x$allow_undefined_flag" = xunsupported; then
  enable_library_bloat=yes
fi
AM_CONDITIONAL([MODULE_DEPENDENCIES],[test "x$enable_library_bloat" = xyes])

#############################################################################
# XDG stuff
AC_CHECK_PROGS([UPDATE_DESKTOP_DATABASE],[update-desktop-database],:)
AC_CHECK_PROGS([UPDATE_MIME_DATABASE],[update-mime-database],:)
GWY_ENABLE([desktop-file-update],
           [desktop_file_update],
           [Update Freedesktop databases])
AM_CONDITIONAL([DESKTOP_FILE_UPDATE],[test "$enable_desktop_file_update" != no])
# This is always supported
THUMBNAILERS="manual"

#############################################################################
# GConf (only for Gnome thumbnailer schemas -- the thumbnailer has other uses
# so we build it unconditionally).
# Very optional.

dnl The following conditional is set in AM_GCONF_SOURCE_2.
dnl Because we may skip its execution, we have to set a default here.
AM_CONDITIONAL([GCONF_SCHEMAS_INSTALL], [false])

dnl Don't publish the GCONFTOOL variable, AM_GCONF_SOURCE_2 has
dnl `gconftool-2' hardwired.
m4_pattern_allow([^AM_GCONF_SOURCE_2$])
GCONFTOOL=
AC_CHECK_PROG([GCONFTOOL],[gconftool-2],[gconftool-2],[no])
if test "x$GCONFTOOL" != xno; then
  AM_GCONF_SOURCE_2
  THUMBNAILERS="$THUMBNAILERS GNOME2"
fi

#############################################################################
# Python
GWY_WITH([python],,[Install Python modules and plug-ins])
AM_PATH_PYTHON(2.2,,:)
AM_CONDITIONAL([HAVE_PYTHON],[test "$PYTHON" != : -a "$enable_python" != no])

AC_ARG_ENABLE(pygwy, AC_HELP_STRING([--enable-pygwy],
                                    [Build Python wrapper to Gwyddion]),[],
                                    [enable_pygwy=yes])

if test "x$enable_pygwy" != xno; then
  if test "$PYTHON" = : -o "x$enable_python" = no; then
    enable_pygwy=no
    PYGWY_WARN=" (needs python)"
  fi
fi

if test "x$enable_pygwy" != xno; then
  GWY_PYTHON_SYSCFG_VAR([BASECFLAGS],,[enable_pygwy=no])
  GWY_PYTHON_SYSCFG_VAR([LDFLAGS],,[enable_pygwy=no])
  GWY_PYTHON_SYSCFG_VAR([CCSHARED],,[enable_pygwy=no])
  GWY_PYTHON_SYSCFG_VAR([LINKFORSHARED],,[enable_pygwy=no])
  if test "x$enable_pygwy" = xno; then
    PYGWY_WARN=" (failed to obtain Python flags)"
  fi
fi

if test "x$enable_pygwy" != xno; then
  AC_CHECK_LIB(python$PYTHON_VERSION, PyRun_String,
               [PYTHON_LIBS=-lpython$PYTHON_VERSION],
               [enable_pygwy=no; PYGWY_WARN=" (needs libpython$PYTHON_VERSION)"],
               [$PYTHON_SYSCFG_LDFLAGS $PYTHON_SYSCFG_LINKFORSHARED])
  AC_SUBST(PYTHON_LIBS)
fi

if test "x$enable_pygwy" != xno; then
  AM_CHECK_PYTHON_HEADERS([],
                          [enable_pygwy=no; PYGWY_WARN=" (needs Python headers)"])
 AC_SUBST(PYTHON_INCLUDES)
fi

if test "x$enable_pygwy" != xno; then
  PKG_CHECK_MODULES(PYGTK,[pygtk-2.0 >= 2.9.0],[],
                    [enable_pygwy=no; PYGWY_WARN=" (needs pygtk2)"])
  AC_SUBST(PYGTK_CFLAGS)
  PYGTK_CODEGENDIR=`$PKG_CONFIG --variable=codegendir pygtk-2.0`
  AC_SUBST(PYGTK_CODEGENDIR)
fi
AM_CONDITIONAL([ENABLE_PYGWY], [test "x$enable_pygwy" != xno])

#############################################################################
# Check for Perl
GWY_WITH([perl],,[Install Perl modules and plug-ins])
AC_PATH_PROGS(PERL,perl5 perl,:)
if test "$PERL" != :; then
  AC_PATH_PROGS(POD2MAN,pod2man,:)
fi
AM_CONDITIONAL([HAVE_PERL], [test "$PERL" != : -a "$enable_perl" != no])

#############################################################################
# Check for Ruby
# The outer [] are necessary to quote the inner [] inside Ruby
GWY_WITH([ruby],,[Install Ruby modules and plug-ins])
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
GWY_WITH([pascal],,[Build sample Pascal plug-ins])
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
  GWY_PROG_CC_OPTION([WNO_UNUSED_PAR],[-Wno-unused-parameter],,)
  GWY_PROG_CC_OPTION([ERR_IFD],[-Werror-implicit-function-declaration],,)
  # FIXME: We would like an error, but gcc can't make one w/o -std, which is
  # too much.
  GWY_PROG_CC_OPTION([WMIXED_DECL],[-Wdeclaration-after-statement],,)
  GWY_PROG_CC_OPTION([WREDUNDANT_DECLS],[-Wredundant-decls],,)
  GWY_PROG_CC_OPTION([FNO_TRAP_MATH],[-fno-trapping-math],,)
  GWY_PROG_CC_OPTION([FNO_MATH_ERRNO],[-fno-math-errno],,)
  # Flags we can't use:
  # -Wstrict-protoypes makes gtkitemfactory.h noisy, but they have a
  #    comment there they can't fix it due to compatibility
  # -Wwrite-strings because it makes all struct initializations very noisy
  WARNING_CFLAGS="-Wall -W -Wshadow -Wpointer-arith -Wno-sign-compare -Wundef $PROG_CC_ERR_IFD $PROG_CC_WNO_SYS_HEADERS $PROG_CC_WNO_POINTER_SIGN $PROG_CC_WNO_EMPTY_FMT $PROG_CC_WMIXED_DECL $PROG_CC_WREDUNDANT_DECLS"
  WARNING_NOUNUSED_PAR="$PROG_CC_WNO_UNUSED_PAR"
  PREMISE_CFLAGS="$PROG_CC_FNO_TRAP_MATH $PROG_CC_FNO_MATH_ERRNO"
fi
AC_SUBST(WARNING_NOUNUSED_PAR)

#############################################################################
# Base libraries: X, Gtk+.
AC_PATH_XTRA
test "x$x_includes" = "x" || CPPFLAGS="$CPPFLAGS -I$x_includes"
test "x$x_libraries" = "x" || LDFLAGS="$LDFLAGS -L$x_libraries"

# XXX: We do not need pangoft2, but we export a deprecated function to obtain
# Pango FT2 context.  Get rid of this once possible.
PKG_CHECK_MODULES(GTK, [pangoft2 pangocairo gtk+-2.0 >= 2.8.0])
PKG_CHECK_MODULES(GOBJECT, [gobject-2.0])
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0])
GWY_CHECK_LIBM

#############################################################################
# FFTW3
# Optional.
FFTW_WARN=
GWY_WITH([fftw3],,[FFTW3 library])
if test "x$enable_fftw3" != "xno"; then
  PKG_CHECK_MODULES(FFTW3, [fftw3],
    [AC_DEFINE(HAVE_FFTW3,1,[Define if we have the FFTW3 package.])],
    [enable_fftw3=no; FFTW_WARN=" (needs FFTW3)"])
fi
if test "x$enable_fftw3" != "xno"; then
  if test "x$arch_32bit" != "xyes"; then
    PKG_CHECK_MODULES(FFTW3_DUMMY,[fftw3 >= 3.1],[:],
      AC_MSG_WARN([FFTW3 older than 3.1 may cause crashes on 64bit systems.
        Consider installation of FFTW3 at least 3.1.  Alternatively you can
        disable FFTW3 with --without-fftw3.])
      FFTW3_WARN=" (with warnings)")
  fi
  FFTW3_DEPENDENCY=fftw3
else
  FFTW3_DEPENDENCY=
fi
AC_SUBST([FFTW3_DEPENDENCY])

#############################################################################
# GtkSourceView
# Optional.
if test "x$enable_pygwy" != xno; then
  GWY_WITH([gtksourceview],,[Better Pygwy console user interface])
  if test "x$enable_gtksourceview" != "xno"; then
    PKG_CHECK_MODULES(GTKSOURCEVIEW,[gtksourceview-2.0],
                      [AC_DEFINE(HAVE_GTKSOURCEVIEW,1,
                                 [Define if we have the GtkSourceView package.])],
                      enable_gtksourceview=no; GTKSOURCEVIEW_WARN=" (needs GtkSourceView)")
  fi
  if test "x$enable_gtksourceview" != "xno"; then
    GTKSOURCEVIEW_DEPENDENCY=gtksourceview-2.0
  else
    GTKSOURCEVIEW_DEPENDENCY=
    GTKSOURCEVIEW_CFLAGS="$GTK_CFLAGS"
    GTKSOURCEVIEW_LIBS="$GTK_LIBS"
  fi
  AC_SUBST([GTKSOURCEVIEW_DEPENDENCY])
fi
#############################################################################
# GtkGLExt
# Optional.
GWY_WITH([gl],,[OpenGL 3D view widgets])
if test "x$enable_gl" != "xno"; then
  # Repeat pangoft2 here to maintain assertion GTK flags are subset of GTKGLEXT
  PKG_CHECK_MODULES(GTKGLEXT,[pangoft2 gtkglext-1.0],
                    [AC_DEFINE(HAVE_GTKGLEXT,1,
                               [Define if we have the GtkGLExt package.])],
                    enable_gl=no; GL_WARN=" (needs GtkGLExt)")
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
# Xmu
# Optional, X11 only.
gdk_target=`$PKG_CONFIG --variable=target gtk+-2.0`
case "$gdk_target" in
  x11)
    have_xmu=no
    if test -z "$LIBXMU"; then
      XMU_ORIG_CFLAGS="$CFLAGS"
      XMU_ORIG_LDFLAGS="$LDFLAGS"
      CFLAGS="$CFLAGS $X_CFLAGS"
      LDFLAGS="$LDFLAGS $X_LIBS"
      AC_CHECK_LIB(Xmu, XmuClientWindow,
        [AC_CHECK_HEADER(X11/Xmu/WinUtil.h,
          [have_xmu=yes;
           LIBXMU="$X_LIBS $X_PRE_LIBS -lX11 -lXmu -lXt";
           AC_DEFINE(HAVE_REMOTE_X11,1,
                     [Define if we have X11 remote control available.])],
          [XMU_WARN=" (needs libXmu headers)"])],
        [XMU_WARN=" (needs libXmu)"],
        [-lXt -lX11])
      CFLAGS="$XMU_ORIG_CFLAGS"
      LDFLAGS="$XMU_ORIG_LDFLAGS"
    else
      have_xmu=yes
    fi
    enable_remote="$have_xmu"
    REMOTE_WARN="$XMU_WARN"
    ;;
  *)
    enable_remote=no
    REMOTE_WARN="(unavailable on $gdk_target)"
    ;;
esac
AC_SUBST(LIBXMU)

#############################################################################
# libz
# Optional.
GWY_WITH([zlib],,[build with zlib support])
if test x$enable_zlib != xno && test -z "$ZLIB"; then
  AC_CHECK_LIB(z, inflate,
    [AC_CHECK_HEADER(zlib.h, ZLIB='-lz', [enable_zlib=no])],
    [enable_zlib=no],
    [])
fi
if test x$enable_zlib != xno && test -n "$ZLIB"; then
  AC_DEFINE(HAVE_ZLIB,1,[Define if we have the ZLIB library.])
else
  ZLIB_WARN=" (needs zlib)"
fi
AM_CONDITIONAL([HAVE_ZLIB],[test x$enable_zlib != xno && test -n "$ZLIB"])
AC_SUBST(ZLIB)

#############################################################################
# SPML importer
# Only if you want to have support for spml importer compiled
GWY_WITH([spml],,[SPML importer (libxml2 required)])
if test "x$enable_spml" != "xno"; then
  PKG_CHECK_MODULES(XML2, [libxml-2.0 >= 2.6],
                    [AC_DEFINE(HAVE_XML2,1,
                               [Define if we have the libxml2 package.])],
                    enable_spml=no; SPML_WARN=" (needs libxml2)")

fi
AM_CONDITIONAL([HAVE_XML2],[test x$enable_spml != xno])

#############################################################################
# KDE4 and Qt4 (for KDE thumbnailer plug-in)
# Very optional.
#
# Note we need headers of both, but actually link only with Qt.
#
# KDE4 uses cmake as the build system.  So either we are going to break things
# by mixing automake & cmake (because the thumbnailer needs to be linked with
# gwyddion libs), or we are going to break things by doing everything manually.
# In the latter case the poor user should have a better chance to fix it by
# setting CXXFLAGS, LIBS, etc. herself.
GWY_WITH([kde4-thumbnailer],
         [kde4_thumbnailer],
         [KDE4 thumbnail creator module])
# Normalize
if test "x$enable_kde4_thumbnailer" != xno; then
  enable_kde4_thumbnailer=yes
fi

# Find kde4-config.
AC_ARG_VAR([KDE4_CONFIG],[path to kde4-config utility])
if test "x$ac_cv_env_KDE4_CONFIG_set" != xset; then
  AC_PATH_TOOL([KDE4_CONFIG],[kde4-config])
fi
if test "x$KDE4_CONFIG" = x; then
  enable_kde4_thumbnailer=no
else
  # XXX: This leaves lots to desire.  What if someone installs Gwyddion to
  # home?  Let her at least overrride KDE4_MODULE_DIR
  KDE4_MODULE_DIR=`$KDE4_CONFIG --install module`
  KDE4_PREFIX=`$KDE4_CONFIG --prefix`
  KDE4_LIBSUFFIX=`$KDE4_CONFIG --libsuffix`
  QT4_PREFIX=`$KDE4_CONFIG --qt-prefix`
  QT4_LIBDIR=`$KDE4_CONFIG --qt-libraries`
fi

AC_LANG_PUSH([C++])
KDE4_ORIG_LIBS="$LIBS"
KDE4_ORIG_LDFLAGS="$LDFLAGS"
KDE4_ORIG_CPPFLAGS="$CPPFLAGS"
KDE4_CPPFLAGS="-I$KDE4_PREFIX/include/kde4 -I$QT4_PREFIX/include"
KDE4_LDFLAGS="-L$KDE4_PREFIX/lib$KDE4_LIBSUFFIX -L$QT4_LIBDIR"
KDE4_LIBS="-lQtCore -lQtGui"
CPPFLAGS="$CPPFLAGS $KDE4_CPPFLAGS"
LDFLAGS="$LDFLAGS $KDE4_LDFLAGS"
LIBS="$LIBS $KDE4_LIBS"

# Try to link with QtCore and QtGui.
if test "x$enable_kde4_thumbnailer" != xno; then
  AC_MSG_CHECKING([for qVersion in $KDE4_LIBS])
  LIBS="$LIBS $KDE4_LIBS"
  AC_LINK_IFELSE([char qVersion(void); int main(void){ return qVersion(); }],
                 [:],[enable_kde4_thumbnailer=no])
  AC_MSG_RESULT([$enable_kde4_thumbnailer])
fi

# Try to compile with the corresponding headers.
if test "x$enable_kde4_thumbnailer" != xno; then
  AC_CHECK_HEADER([QtCore/qstring.h],[:],[enable_kde4_thumbnailer=no])
fi
if test "x$enable_kde4_thumbnailer" != xno; then
  AC_CHECK_HEADER([QtGui/qimage.h],[:],[enable_kde4_thumbnailer=no])
fi
if test "x$enable_kde4_thumbnailer" != xno; then
  AC_CHECK_HEADER([kio/thumbcreator.h],[:],[enable_kde4_thumbnailer=no])
fi

CPPFLAGS="$KDE4_ORIG_CPPFLAGS"
LDFLAGS="$KDE4_ORIG_LDFLAGS"
LIBS="$KDE4_ORIG_LIBS"
AC_LANG_POP([C++])

AM_CONDITIONAL([KDE4_THUMBNAILER],[test "x$enable_kde4_thumbnailer" != xno])
if test "x$enable_kde4_thumbnailer" != xno; then
  THUMBNAILERS="$THUMBNAILERS KDE4"
fi

AC_SUBST(KDE4_MODULE_DIR)
AC_SUBST(KDE4_CPPFLAGS)
AC_SUBST(KDE4_LDFLAGS)
AC_SUBST(KDE4_LIBS)

#############################################################################
# Check for header files.
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

#############################################################################
# Check for library functions.
AC_FUNC_MMAP
GWY_CHECK_MATH_FUNCS([cbrt hypot pow10 acosh asinh atanh])
AC_CHECK_FUNCS([sincos _fpclass])
dnl FIXME: We need C99 for this.  But we must be able to switch off its
dnl aliasing rules which at this moment we can do for GCC only.  That's too
dnl limited.
dnl GWY_CHECK_FPCLASSIFY

#############################################################################
# I18n
GETTEXT_PACKAGE=$PACKAGE_TARNAME
AM_GNU_GETTEXT([external])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])
AC_SUBST(GETTEXT_PACKAGE)

#############################################################################
# GLIBC >= 2.1
gl_GLIBC21
if test "x$GLIBC21" = xyes; then
  AC_DEFINE([HAVE_MEMMEM],1,[Define if we have working memmem()])
fi

#############################################################################
# Libs
BASIC_LIBS="$GOBJECT_LIBS $INTLLIBS $LIBM"
LIBS="$ORIG_LIBS"
AC_SUBST(BASIC_LIBS)
COMMON_LDFLAGS="$HOST_LDFLAGS"
AC_SUBST(COMMON_LDFLAGS)

COMMON_CFLAGS="$HOST_CFLAGS $WARNING_CFLAGS $GOBJECT_CFLAGS $GMODULE_CFLAGS $GTKGLEXT_CFLAGS $PREMISE_CFLAGS"
AC_SUBST(COMMON_CFLAGS)

#############################################################################
# Plugin-proxy
# Deprecated.
GWY_ENABLE([plugin-proxy],
           [plugin_proxy],
           [Build plug-in proxy module])
AM_CONDITIONAL([ENABLE_PLUGIN_PROXY],[test "$enable_plugin_proxy" != no])

#############################################################################
# RPM
AC_DEFINE_UNQUOTED(PACKAGE_URL,"$PACKAGE_URL",
                   [Define to package home page, as a string.])
AC_SUBST(PACKAGE_URL)

#############################################################################
# Generate gwyconfig
#
# NB: When you add definitions here, add them to data/gwyconfig.h.win32 too.
#
# FIXME: #define-ing symbols from libc is quite arrogant, even if they are
# missing on the system.  It can conflict with other packages.  Define
# something like gwy_foo() and use it instead of foo() in the source code,
# or, alternatively, make these definitions private.
#
AC_CONFIG_COMMANDS([gwyconfig.h],
[
  outfile=gwyconfig.h-tmp
  cat > $outfile <<\_______EOF
/*
 * gwyconfig.h
 *
 * This is a generated file.  Please modify 'configure.ac'.
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

  echo '/* Math functions become available when gwymathfallback.h is included. */' >>$outfile
  echo '/* In 2.x also when gwymath.h is included. */' >>$outfile

GWY_OUTPUT_MATH_FUNCS([cbrt hypot pow10 acosh asinh atanh], [$outfile])
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
  gwy_math_pow10=$gwy_math_pow10
  gwy_math_hypot=$gwy_math_hypot
  gwy_math_cbrt=$gwy_math_cbrt
  gwy_math_acosh=$gwy_math_acosh
  gwy_math_asinh=$gwy_math_asinh
  gwy_math_atanh=$gwy_math_atanh
])

AC_OUTPUT

echo "================================================================="
echo "Configuration:"
echo "  SMPL:                              $enable_spml$SPML_WARN"
#echo "  Matlab MAT5:                       $enable_zlib$ZLIB_WARN"
echo "  FFTW3:                             $enable_fftw3$FFTW3_WARN"
echo "  OpenGL 3D widgets:                 $enable_gl$GL_WARN"
echo "  Remote control:                    $enable_remote$REMOTE_WARN"
echo "  Thumbnailers to build:             $THUMBNAILERS"
echo "  Python interface (pygwy):          $enable_pygwy$PYGWY_WARN"
if test x$enable_pygwy = xyes; then
  echo "  Pygwy console syntax highlighting: $enable_gtksourceview$GTKSOURCEVIEW_WARN"
fi
echo "================================================================="

