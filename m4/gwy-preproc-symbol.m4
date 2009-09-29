dnl (@) $Id$
dnl GWY_PREPROC_SYMBOL(SYMBOL, ALIAS, INCLUDES, CPPFLAGS)
dnl Define preprocesor symbol obtained from a particular set of includes.
dnl Useful when dealing with conflicting sets of headers.
AC_DEFUN([GWY_PREPROC_SYMBOL],
[AC_REQUIRE([AC_PROG_CPP])dnl
AC_CACHE_CHECK([value of preprocessor symbol $1], ac_cv_c_preproc_symbol_$2,
[save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $4"
cat >conftest.h <<_ACEOF
$3
<<$1>>
_ACEOF
(eval "$ac_cpp conftest.h") 2>conftest.err >conftest.out
ac_cv_c_preproc_symbol_$2=`sed -e '$s/<<\(.*\)>>/\1/' -e t -e d conftest.out`
rm -f conftest.h conftest.err conftest.out
])
AC_DEFINE_UNQUOTED([PREPROC_SYMBOL_$2],[$ac_cv_c_preproc_symbol_$2],[$1])
])

