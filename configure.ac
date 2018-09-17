dnl Process this file with autoconf 2.61 or later to produce
dnl a configure script.
AC_PREREQ([2.69])

AC_CONFIG_MACRO_DIR(autoconf)

AC_INIT
AC_CONFIG_SRCDIR([abc80.c])
AC_CONFIG_HEADERS(config.h)

AC_CONFIG_AUX_DIR(autoconf)

dnl If we have wine installed, don't run it when checking for cross-compile
WINELOADER=/dev/null
export WINELOADER

AC_PREFIX_PROGRAM(abc80)

dnl Get canonical target ("host") name
AC_CANONICAL_HOST

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

dnl Search files
AC_CHECK_FUNCS([popen])

dnl See if we can find sdl-config
AC_CHECK_TOOL([SDL_CONFIG], [sdl-config], [false])
AS_IF(["$SDL_CONFIG" --cflags >/dev/null 2>&1],
      [SDL_CFLAGS="`"$SDL_CONFIG" --cflags`"
       SDL_LIBS="`"$SDL_CONFIG" --libs`"],
      [AC_MSG_WARN([sdl-config not found or not functional, this is probably bad news])])
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

dnl Checks for library functions.
AC_SEARCH_LIBS([deflate], [z])
AC_SEARCH_LIBS([png_write_png], [png])

dnl
dnl support cchace
dnl
AC_ARG_WITH([ccache],
            [AS_HELP_STRING([--with-ccache],[Compile with ccache])],
          [CC="ccache $CC"],
          [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
