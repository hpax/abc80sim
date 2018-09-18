dnl --------------------------------------------------------------------------
dnl PA_SYM(prefix, string)
dnl
dnl Convert a (semi-) arbitrary string to a CPP symbol
dnl --------------------------------------------------------------------------
AC_DEFUN(PA_SYM,
[[$1]m4_bpatsubsts(m4_toupper([$2]),[[^ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789]+],[_],[^._?\(.*\)_.$],[[\1]])])

dnl --------------------------------------------------------------------------
dnl PA_ADD_FLAGS(variable, flag [,actual_flag])
dnl
dnl Attempt to add the given option to CPPFLAGS, if it doesn't break
dnl compilation.  If the option to be tested is different than the
dnl option that should actually be added, add the option to be
dnl actually added as a second argument.
dnl --------------------------------------------------------------------------
AC_DEFUN(PA_ADD_FLAGS,
[AC_MSG_CHECKING([if $CC accepts $2])
 pa_add_flags__old_flags="$$1"
 $1="$$1 $2"
 AC_TRY_LINK(AC_INCLUDES_DEFAULT,
 [printf("Hello, World!\n");],
 [AC_MSG_RESULT([yes])
  $1="$pa_add_flags__old_flags ifelse([$3],[],[$2],[$3])"
  AC_DEFINE(PA_SYM([$1_],[$2]), 1,
   [Define to 1 if compiled with the `$2' compiler flag])],
 [AC_MSG_RESULT([no])
  $1="$pa_add_flags__old_flags"])])

dnl --------------------------------------------------------------------------
dnl PA_ADD_{CPP,C,LD}FLAGS(variable, flag [,actual_flag])
dnl
dnl Attempt to add the given option to xFLAGS, if it doesn't break
dnl compilation.  If the option to be tested is different than the
dnl option that should actually be added, add the option to be
dnl actually added as a second argument.
dnl --------------------------------------------------------------------------
AC_DEFUN(PA_ADD_CPPFLAGS, [PA_ADD_FLAGS(CPPFLAGS, [$1], [$2])])
AC_DEFUN(PA_ADD_CFLAGS,   [PA_ADD_FLAGS(CFLAGS, [$1], [$2])])
AC_DEFUN(PA_ADD_LDFLAGS,  [PA_ADD_FLAGS(LDFLAGS, [$1], [$2])])

dnl --------------------------------------------------------------------------
dnl PA_HAVE_FUNC(func_name)
dnl
dnl Look for a function with the specified arguments which could be
dnl a builtin/intrinsic function.
dnl --------------------------------------------------------------------------
AC_DEFUN(PA_HAVE_FUNC,
[AC_MSG_CHECKING([for $1])
 AC_LINK_IFELSE([AC_LANG_SOURCE([
AC_INCLUDES_DEFAULT
int main(void) {
    (void)$1$2;
    return 0;
}
 ])],
 [AC_MSG_RESULT([yes])
  AC_DEFINE(AS_TR_CPP([HAVE_$1]), 1,
  [Define to 1 if you have the `$1' intrinsic function.])],
 [AC_MSG_RESULT([no])])
])

dnl --------------------------------------------------------------------------
dnl PA_FUNC_ATTRIBUTE(attribute_name)
dnl
dnl See if this compiler supports the equivalent of a specific gcc
dnl attribute on a function, using the __attribute__(()) syntax.
dnl All arguments except the attribute name are optional.
dnl PA_FUNC_ATTRIBUTE(attribute, attribute_opts, return_type,
dnl                   prototype_args, call_args)
dnl --------------------------------------------------------------------------
AC_DEFUN(PA_FUNC_ATTRIBUTE,
[AC_MSG_CHECKING([if $CC supports the $1 function attribute])
 AC_COMPILE_IFELSE([AC_LANG_SOURCE([
AC_INCLUDES_DEFAULT
extern ifelse([$3],[],[void *],[$3])  __attribute__(($1$2))
  bar(ifelse([$4],[],[int],[$4]));
ifelse([$3],[],[void *],[$3]) foo(void);
ifelse([$3],[],[void *],[$3]) foo(void)
{
	ifelse([$3],[void],[],[return])
		bar(ifelse([$5],[],[1],[$5]));
}
 ])],
 [AC_MSG_RESULT([yes])
  AC_DEFINE(PA_SYM([HAVE_FUNC_ATTRIBUTE_],[$1]), 1,
    [Define to 1 if your compiler supports __attribute__(($1)) on functions])],
 [AC_MSG_RESULT([no])])
])

dnl --------------------------------------------------------------------------
dnl PA_FUNC_ATTRIBUTE_ERROR
dnl
dnl See if this compiler supports __attribute__((error("foo")))
dnl The generic version of this doesn't work as it makes the compiler
dnl throw an error by design.
dnl --------------------------------------------------------------------------
AC_DEFUN(PA_FUNC_ATTRIBUTE_ERROR,
[AC_MSG_CHECKING([if $CC supports the error function attribute])
 AC_COMPILE_IFELSE([AC_LANG_SOURCE([
AC_INCLUDES_DEFAULT
extern void __attribute__((error("message"))) barf(void);
void foo(void);
void foo(void)
{
	if (0)
		barf();
}
 ])],
 [AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_FUNC_ATTRIBUTE_ERROR], 1,
    [Define to 1 if your compiler supports __attribute__((error)) on functions])],
 [AC_MSG_RESULT([no])])
])

dnl --------------------------------------------------------------------------
dnl PA_ARG_ENABLED(option, helptext [,enabled_action [,disabled_action]])
dnl PA_ARG_DISABLED(option, helptext [,disabled_action [,enabled_action]])
dnl
dnl  Simpler-to-use versions of AC_ARG_ENABLED, that include the
dnl  test for $enableval and the AS_HELP_STRING definition
dnl --------------------------------------------------------------------------
AC_DEFUN(PA_ARG_ENABLED,
[AC_ARG_ENABLE([$1], [AS_HELP_STRING([--enable-$1],[$2])], [], [enableval=no])
 AS_IF([test x"$enableval" != xno], [$3], [$4])
])

AC_DEFUN(PA_ARG_DISABLED,
[AC_ARG_ENABLE([$1],[AS_HELP_STRING([--disable-$1],[$2])], [], [enableval=yes])
 AS_IF([test x"$enableval" = xno], [$3], [$4])
])

dnl --------------------------------------------------------------------------
dnl PA_ADD_HEADERS(headers...)
dnl
dnl Call AC_CHECK_HEADERS(), and add to ac_includes_default if found
dnl --------------------------------------------------------------------------
AC_DEFUN(_PA_ADD_HEADER,
[AC_CHECK_HEADERS([$1],[ac_includes_default="$ac_includes_default
#include <$1>"
])])

AC_DEFUN(PA_ADD_HEADERS,
[m4_map_args_w([$1],[_PA_ADD_HEADER(],[)])])

dnl --------------------------------------------------------------------------
dnl PA_CHECK_BAD_STDC_INLINE
dnl
dnl Some versions of gcc seem to apply -Wmissing-prototypes to C99
dnl inline functions, which means we need to use GNU inline syntax
dnl --------------------------------------------------------------------------
AC_DEFUN(PA_CHECK_BAD_STDC_INLINE,
[AC_MSG_CHECKING([if $CC supports C99 external inlines])
 AC_COMPILE_IFELSE([AC_LANG_SOURCE([
AC_INCLUDES_DEFAULT

/* Don't mistake GNU inlines for c99 */
#ifdef __GNUC_GNU_INLINE__
# error "Using gnu inline standard"
#endif

inline int foo(int x)
{
	return x+1;
}
 ])],
 [AC_MSG_RESULT([yes])
  AC_DEFINE(HAVE_STDC_INLINE, 1,
    [Define to 1 if your compiler supports C99 extern inline])],
 [AC_MSG_RESULT([no])
  PA_ADD_CFLAGS([-fgnu89-inline])])])
