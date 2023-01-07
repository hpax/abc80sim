dnl --------------------------------------------------------------------------
dnl PA_FLAGS_LANGLIST(flagvar)
dnl
dnl Return a list of languages affected by the variable flagvar
dnl --------------------------------------------------------------------------
AC_DEFUN([PA_ADD_FLAGS],
[
  AS_VAR_PUSHDEF(old, [_$0_$1_orig])
  AS_VAR_PUSHDEF(ok, [_$0_$1_ok])
  AS_VAR_PUSHDEF(flags, [$1])

  AS_VAR_SET([old], ["$flags"])
  AS_VAR_SET([flags], ["$flags $2"])
  AS_VAR_SET([ok], [yes])
  m4_pushdef([_pa_add_flags_old_lang],_AC_LANG)

  m4_foreach([_pa_add_flags_lang],
  PA_FLAGS_LANGLIST($1),
  [PA_LANG_SEEN(_pa_add_flags_lang,
   [PA_LANG(_pa_add_flags_lang)
    AS_VAR_IF([ok], [yes],
    [AC_MSG_CHECKING([if $]_AC_CC[ accepts $2])
     PA_BUILD_IFELSE([],
     [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AS_VAR_SET([ok], [no])])])
     ])])

 PA_LANG(_pa_add_flags_old_lang)
 m4_popdef([_pa_add_flags_old_lang])

 AS_VAR_IF([ok], [yes],
  [m4_ifnblank([$3],[AS_VAR_SET([flags], ["$old $3"])])
   m4_foreach_w([_pa_add_flags_flag], [m4_ifblank([$3],[$2],[$3])],
   [AC_DEFINE(PA_SYM([$1_]_pa_add_flags_flag), 1,
    [Define to 1 if compiled with the ]_pa_add_flags_flag[ compiler flag])])
   $4],
  [AS_VAR_SET([flags], ["$old"])
   $5])

  AS_VAR_POPDEF([flags])
  AS_VAR_POPDEF([ok])
  AS_VAR_POPDEF([old])
])
