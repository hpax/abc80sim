dnl --------------------------------------------------------------------------
dnl PA_CC_AUTODEPS
dnl
dnl If the C compiler supports -MMD -MF, add a suitable rule to AUTODEPS
dnl --------------------------------------------------------------------------
AC_DEFUN([PA_CC_AUTODEPS],
[AC_MSG_CHECKING([if $CC supports automatic dependency generation])
 pa_add_flags__old_flags="$CPPFLAGS"
 CPPFLAGS="$CPPFLAGS -MMD -MF /dev/null"
 AC_TRY_LINK(AC_INCLUDES_DEFAULT,
  [printf("Hello, World!\n");],
  [AC_MSG_RESULT([yes])
   [AUTODEPS='-MMD -MF @S|@(@D)/.@S|@(@F).d']],
 [AC_MSG_RESULT([no])
  AUTODEPS=''])
 CPPFLAGS="$pa_add_flags__old_flags"
 AC_SUBST([AUTODEPS])])
 

