dnl --------------------------------------------------------------------------
dnl PA_LANG_SEEN(lang, yes, no)
dnl
dnl Expands yes if the language lang has been used in the configuration
dnl script so far, otherwise no
dnl --------------------------------------------------------------------------
AC_DEFUN([PA_LANG_SEEN],[m4_set_contains(PA_LANG_SEEN_SET,[$1],[$2],[$3])])
