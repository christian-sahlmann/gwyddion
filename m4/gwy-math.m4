dnl Simple AC_CHECK_LIBM wrapper that makes possible to use AC_BEFORE
AC_DEFUN([GWY_CHECK_LIBM],
[
AC_BEFORE([GWY_CHECK_MATH_FUNC])dnl
ORIG_LIBS="$LIBS"
AC_CHECK_LIBM
LIBS="$LIBS $LIBM"
])

AC_DEFUN([GWY_CHECK_MATH_FUNC],
[
AC_REQUIRE([AC_CHECK_FUNC])dnl
AC_CHECK_FUNC([$1], [AS_TR_SH([gwy_math_$1])=yes], [AS_TR_SH([gwy_math_$1])=no])
])

AC_DEFUN([GWY_CHECK_MATH_FUNCS],
[
m4_foreach_w([Gwy_Func], [$1], [GWY_CHECK_MATH_FUNC(Gwy_Func)])
])

AC_DEFUN([GWY_OUTPUT_MATH_FUNC],
[
dnl For some reason AS_TR_CPP embedded in the echo does not get expanded.
gwy_math__sym=AS_TR_CPP([GWY_HAVE_$1])
echo >>$2
echo '/* Define to 1 if you have the $1[()] function. */' >>$2
if test x$AS_TR_SH([gwy_math_$1]) = xyes; then
   echo "#define $gwy_math__sym 1" >>$2
else
   echo "#undef $gwy_math__sym" >>$2
fi
])

AC_DEFUN([GWY_OUTPUT_MATH_FUNCS],
[
m4_foreach_w([Gwy_Func], [$1], [GWY_OUTPUT_MATH_FUNC(Gwy_Func, $2)])
])
