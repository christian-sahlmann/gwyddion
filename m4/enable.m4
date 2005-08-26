dnl Create simple --enable option $1, creating enable_$2 with help text $3,
dnl Default for $2 is $1.
AC_DEFUN([GWY_ENABLE],
[
AC_ARG_ENABLE([$1],
  AS_HELP_STRING([--enable-$1],
                 [$3 (yes)]),
  [case "${enableval}" in
     yes|no) ifelse([$2],,[enable_$1="$enableval"],[enable_$2="$enableval"]) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-$1) ;;
   esac],
  [ifelse([$2],,[enable_$1=yes],[enable_$2=yes])])
  export ifelse([$2],,[enable_$1],[enable_$2])
])

dnl Create simple --with option $1, creating enable_$2 with help text $3,
dnl Default for $2 is $1.
AC_DEFUN([GWY_WITH],
[
AC_ARG_WITH([$1],
  AS_HELP_STRING([--with-$1],
                 [$3 (yes)]),
  [case "${withval}" in
     yes|no) ifelse([$2],,[enable_$1="$withval"],[enable_$2="$withval"]) ;;
     *) AC_MSG_ERROR(bad value ${withval} for --with-$1) ;;
   esac],
  [ifelse([$2],,[enable_$1=yes],[enable_$2=yes])])
  export ifelse([$2],,[enable_$1],[enable_$2])
])
