dnl --------------------------------------------------------------------------
dnl PA_CC_AUTODEPS
dnl
dnl If the C compiler supports -MMD -MF, add a suitable rule to AUTODEPS
dnl XXX: fix languages
dnl --------------------------------------------------------------------------
AC_DEFUN([PA_CC_AUTODEPS],
[PA_ARG_BOOL(dependency-tracking,
[source dependency tracking],
[yes],
[AC_MSG_CHECKING([if $]_AC_CC[ supports automatic dependency generation])
 pa_add_flags__old_flags="$CPPFLAGS"
 rm -f conftest.dep
 CPPFLAGS="$pa_add_flags__old_flags -MMD -MF conftest.dep"
 AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],
    [printf("Hello, World!\n");])],
    [deptest=yes], [deptest=no])
  AS_IF([test -s conftest.dep], [], [deptest=no])
  rm -f conftest.dep
  AS_IF([test x$deptest = xyes],
  [AC_MSG_RESULT([yes])
   AUTODEPS='-MMD -MF @S|@(@D)/.@S|@(@F).d'],
  [AC_MSG_RESULT([no])
   AUTODEPS=''])
  CPPFLAGS="$pa_add_flags__old_flags"])
AC_SUBST([AUTODEPS])])
