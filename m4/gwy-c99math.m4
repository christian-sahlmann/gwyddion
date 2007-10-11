dnl fpclassify() can be a function or macro or whatever.
AC_DEFUN([GWY_CHECK_FPCLASSIFY],
[
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([#include <math.h>],
                   [double x = 0.0; return fpclassify(x) != FP_NORMAL;])],
  [AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <math.h>],
                     [double x = 0.0; return fpclassify(x) != FP_NORMAL;])],
    [AC_DEFINE([HAVE_FPCLASSIFY], [1], [Define if you have the fpclassify() function/macro.])]
    )])
])
