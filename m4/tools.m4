## Set directories containing automake m4 macros (bootstraping).
## Defines: (nothing)
dnl AM_ACLOCAL_INCLUDE(macrodir)
AC_DEFUN([AM_ACLOCAL_INCLUDE],
[dnl Append aclocal flags and then add all specified dirs.
  test -n "$ACLOCAL_FLAGS" && ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"
  for k in $1 ; do ACLOCAL="$ACLOCAL -I $k" ; done
])

