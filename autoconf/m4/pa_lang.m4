dnl --------------------------------------------------------------------------
dnl PA_LANG(lang)
dnl
dnl Like AC_LANG() but conditional (is this a bug fix?)
dnl --------------------------------------------------------------------------
AC_DEFUN([PA_LANG], [m4_if(_AC_LANG,[$1],[],[AC_LANG([$1])])])
