dnl --------------------------------------------------------------------------
dnl PA_LANG_SEEN_SET
dnl
dnl Set of the language lang has been used in the configuration
dnl script so far
dnl --------------------------------------------------------------------------
m4_ifndef([_PA_LANG_SET],
[m4_copy([_AC_LANG_SET], [_PA_LANG_SET])
m4_defun([_AC_LANG_SET],
 [m4_set_add([_PA_LANG_SEEN_SET],[$2])dnl
_PA_LANG_SET($@)])])

AC_DEFUN([PA_LANG_SEEN_SET], [[_PA_LANG_SEEN_SET]])
