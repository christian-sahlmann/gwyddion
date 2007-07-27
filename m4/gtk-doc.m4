# serial 1

dnl Usage:
dnl   GTK_DOC_CHECK([minimum-gtk-doc-version])
AC_DEFUN([GTK_DOC_CHECK],
[
  AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
  AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
  dnl for overriding the documentation installation directory
  AC_ARG_WITH([html-dir],
    AS_HELP_STRING([--with-html-dir=PATH], [path to installed docs]),,
    [with_html_dir='${datadir}/gtk-doc/html'])
  HTML_DIR="$with_html_dir"
  AC_SUBST([HTML_DIR])

  dnl enable/disable documentation building
  AC_ARG_ENABLE([gtk-doc],
    AS_HELP_STRING([--enable-gtk-doc],
                   [use gtk-doc to build documentation [[default=no]]]),,
    [enable_gtk_doc=no])

  if test x$enable_gtk_doc = xyes; then
    ifelse([$1],[],
      [PKG_CHECK_EXISTS([gtk-doc],,
                        AC_MSG_ERROR([gtk-doc not installed and --enable-gtk-doc requested]))],
      [PKG_CHECK_EXISTS([gtk-doc >= $1],,
                        AC_MSG_ERROR([You need to have gtk-doc >= $1 installed to build gtk-doc]))])
  fi

  if test x$enable_gtk_doc = xyes; then
    AC_MSG_CHECKING([for gtk-doc data path])
    # Unfortunately gtk-doc does not offer more detailed paths
    GTK_DOC_PATH=`$PKG_CONFIG --variable=prefix gtk-doc`
    GTK_DOC_PATH="$GTK_DOC_PATH/share/gtk-doc"
    AC_MSG_RESULT([$GTK_DOC_PATH])
    if test ! -f "$GTK_DOC_PATH/data/gtk-doc.xsl"; then
      AC_MSG_WARN([$GTK_DOC_PATH does not contain data/gtk-doc.xsl!])
      enable_gtk_doc=no
    fi
  fi

  AC_MSG_CHECKING([whether to build gtk-doc documentation])
  AC_MSG_RESULT($enable_gtk_doc)

  AM_CONDITIONAL([ENABLE_GTK_DOC], [test x$enable_gtk_doc = xyes])
  AM_CONDITIONAL([GTK_DOC_USE_LIBTOOL], [test -n "$LIBTOOL"])
  AC_SUBST([GTK_DOC_PATH])
])

