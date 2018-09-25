dnl --------------------------------------------------------------------------
dnl PA_SANE_TERMIOS
dnl
dnl See if the speed constants in <termios.h> appear to be real numbers
dnl --------------------------------------------------------------------------
AC_DEFUN([_PA_TERMIOS_IS_SANE],
[AC_MSG_CHECKING([if termios.h speed constants are sane])
 AC_COMPILE_IFELSE([
  AC_LANG_PROGRAM([
AC_INCLUDES_DEFAULT
#include <termios.h>
],
[m4_foreach_w([pa_bxx],[50 75 110 134 150 200 300 600 1200 1800 2400 4800 9600
  19200 38400 57600 115200 230400 460800 500000 576000 921600 1000000
  1152000 1500000 2000000 2500000 3000000 3500000 4000000],[m4_do([
#ifdef B]pa_bxx[
# if B]pa_bxx[ != ]pa_bxx[
#  error "B]pa_bxx[ is insane"
# endif
#endif
])])])],
 [AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_SANE_TERMIOS],[1],
    [Define to 1 if <termios.h> Bxx constants are simply numbers])],
 [AC_MSG_RESULT([no])])])

AC_DEFUN([PA_SANE_TERMIOS],
[AC_CHECK_HEADERS([termios.h],
 [_PA_TERMIOS_IS_SANE
 AC_CHECK_MEMBERS([struct termios.c_ispeed], [], [], [
AC_INCLUDES_DEFAULT
#include <termios.h>
])])])

