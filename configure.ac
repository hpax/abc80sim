dnl Process this file with autoconf 2.61 or later to produce
dnl a configure script.
AC_PREREQ(2.61)
AC_INIT(config.h.in)
AC_CONFIG_HEADERS(config.h)

AC_PREFIX_PROGRAM(abc80)

dnl Checks for programs.
dnl Consider AC_USE_SYSTEM_EXTENSIONS if autoconf 2.61 is OK in the future
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_INSTALL

dnl If we have gcc, add appropriate options
PA_ADD_CFLAGS([-W])
PA_ADD_CFLAGS([-Wall])
PA_ADD_CFLAGS([-std=c99])
PA_ADD_CFLAGS([-pedantic])

dnl Checks for header files.
AC_HEADER_STDC

dnl The standard header for str*casecmp is <strings.h>
AC_CHECK_HEADERS(strings.h)

dnl Look for <stdbool.h>
AC_CHECK_HEADERS(stdbool.h)

AC_CHECK_HEADERS(SDL.h)
AC_CHECK_HEADERS(SDL/SDL.h)
AC_CHECK_HEADERS(png.h)

AC_CHECK_HEADERS(dirent.h)
AC_CHECK_HEADERS(sys/types.h)
AC_CHECK_HEADERS(sys/stat.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(io.h)
AC_CHECK_HEADERS(windows.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_SIZE_T
PA_WORKING_BOOL
AC_C_BIGENDIAN(AC_DEFINE(WORDS_BIGENDIAN),AC_DEFINE(WORDS_LITTLEENDIAN))
AH_TEMPLATE(WORDS_BIGENDIAN,
[Define to 1 if your processor stores words with the most significant
byte first (like Motorola and SPARC, unlike Intel and VAX).])
AH_TEMPLATE(WORDS_LITTLEENDIAN,
[Define to 1 if your processor stores words with the least significant
byte first (like Intel and VAX, unlike Motorola and SPARC).])

dnl Checks for library functions.
AC_SEARCH_LIBS([png_write_png], [png])
AC_SEARCH_LIBS([SDL_Init], [SDL])

dnl I am going to assume modern systems don't need to guess
dnl what our dependent libraries may be...
dnl AC_SEARCH_LIBS([deflate], [z])
dnl AC_SEARCH_LIBS([pthread_create], [pthread])

dnl
dnl support cchace
dnl
AC_ARG_WITH([ccache],
            [AC_HELP_STRING([--with-ccache],
                            [Compile with ccache])],
          [CC="ccache $CC"],
          [])

AC_OUTPUT(Makefile)
