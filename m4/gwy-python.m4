dnl GWY_PYTHON_SYSCFG_VAR(VARIABLE [,ACTION-ON-SUCCESS [,ACTION-ON-FAILURE]])
dnl VARIABLE is a distutils.sysconfig variable such as LINKFORSHARED;
dnl          output variable PYTHON_SYSCFG_${VARIABLE} is always created,
dnl          filled with the value of VARIABLE on success, left empty on
dnl          failure
dnl ACTION-ON-SUCCESS is executed when the value of VARIABLE was successfully
dnl                   obtained
dnl ACTION-ON-FAILURE is executed on any kind of failure (can't run Python
dnl                   or anything)
dnl
dnl Requires: AM_PATH_PYTHON, as you probably want a particular minimum Python
dnl           version, call it yourself beforehand
AC_DEFUN([GWY_PYTHON_SYSCFG_VAR],
[
AC_REQUIRE([AM_PATH_PYTHON])dnl
AC_SUBST([PYTHON_SYSCFG_$1])
AC_MSG_CHECKING([for python build option $1])
if test -n "$PYTHON_SYSCFG_$1"; then
  AC_MSG_RESULT([$PYTHON_SYSCFG_$1])
  export PYTHON_SYSCFG_$1
  $2
else
cat >conftest.py <<\_______EOF
import sys, distutils.sysconfig
x = sys.argv[[1]].strip()
v = distutils.sysconfig.get_config_var(x)
if not isinstance(v, str):
    sys.stderr.write('Value of %s is not a string' % x)
    sys.exit(1)
print 'PYTHON_SYSCFG_%s="%s"' % (x, v)
print 'ac_res="%s"' % v
_______EOF

if $PYTHON conftest.py $1 >conftest.file 2>conftest.err && test ! -s conftest.err; then
  eval `cat conftest.file`
  AC_MSG_RESULT([$ac_res])
  $2
else
  AC_MSG_RESULT([unknown])
  cat conftest.err >&5
  echo "$as_me: failed program was:" >&5
  sed 's/^/| /' conftest.py >&5
  eval PYTHON_SYSCFG_$1=
  $3
fi
rm -f conftest.py conftest.err conftest.file
fi
])

## Find the install dirs for the python installation.
##  By James Henstridge
##  Fixed by Yeti because does not work on MS Windows

dnl a macro to check for ability to create python extensions
dnl  AM_CHECK_PYTHON_HEADERS([ACTION-IF-POSSIBLE], [ACTION-IF-NOT-POSSIBLE])
dnl function also defines PYTHON_INCLUDES
AC_DEFUN([GWY_CHECK_PYTHON_HEADERS],
[AC_REQUIRE([AM_PATH_PYTHON])
AC_MSG_CHECKING(for headers required to compile python extensions)
dnl deduce PYTHON_INCLUDES
py_prefix=`$PYTHON -c "import sys; sys.stdout.write(sys.prefix.replace('\\\\\\\\','/'))"`
py_exec_prefix=`$PYTHON -c "import sys; sys.stdout.write(sys.exec_prefix.replace('\\\\\\\\','/'))"`
PYTHON_INCLUDES="-I${py_prefix}/include/python${PYTHON_VERSION}"
if test "$py_prefix" != "$py_exec_prefix"; then
  PYTHON_INCLUDES="$PYTHON_INCLUDES -I${py_exec_prefix}/include/python${PYTHON_VERSION}"
fi
AC_SUBST(PYTHON_INCLUDES)
dnl check if the headers exist:
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $PYTHON_INCLUDES"
AC_TRY_CPP([#include <Python.h>],dnl
[AC_MSG_RESULT(found)
$1],dnl
[AC_MSG_RESULT(not found)
$2])
CPPFLAGS="$save_CPPFLAGS"
])

