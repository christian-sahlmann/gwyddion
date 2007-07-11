dnl GWY_PYTHON_SYSCFG_VAR(VARIABLE [,ACTION-ON-SUCCESS [,ACTION-ON-FAILURE]])
dnl VARIABLE is a distutils.sysconfig variable such as LINKFORSHARED;
dnl          output variable PYTHON_SYSCFG_${VARIABLE} is always created,
dnl          filled with the value of VARIABLE on success, left empty on
dnl          failure
dnl ACTION-ON-SUCCESS is executed when the value of VARIABLE was successfully
dnl                   obtained
dnl ACTION-ON-FAILURE is executed on any kind of failure (can't run Python
dnl                   or anything
dnl
dnl Requires: AM_PATH_PYTHON, since you want a particular minimum Python
dnl           version, call it yourself before
AC_DEFUN([GWY_PYTHON_SYSCFG_VAR],
[
AC_REQUIRE([AM_PATH_PYTHON])dnl
AC_SUBST([PYTHON_SYSCFG_$1])
AC_MSG_CHECKING([for python build option $1])
cat >conftest.py <<\_______EOF
import sys, distutils.sysconfig
x = sys.argv[[1]].strip()
v = distutils.sysconfig.get_config_var(x)
if type(v) != type('abcd'):
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
])

