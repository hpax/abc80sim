dnl Process this file with autoconf 2.69 or later to produce
dnl a configure script.
AC_PREREQ([2.69])

AC_CONFIG_MACRO_DIR(autoconf)
AC_CONFIG_MACRO_DIRS(autoconf)

AC_INIT(abc80.c)
AC_CONFIG_HEADERS(config/config.h)

AC_CONFIG_AUX_DIR(autoconf/aux)

dnl If we have wine installed, don't run it when checking for cross-compile
WINELOADER=/dev/null
export WINELOADER

AC_PREFIX_PROGRAM(abc80)

dnl What to build
TARGETS='abc80$(X)'
AC_SUBST(TARGETS)

dnl Get canonical target ("host") name
AC_CANONICAL_HOST

dnl Checks for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_INSTALL

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_SIZE_T

dnl Checks for standard header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_CHECK_HEADERS(inttypes.h)
AC_CHECK_HEADERS(stdnoreturn.h)

dnl Force gcc and gcc-compatible compilers treat signed integers
dnl as 2's complement
PA_ADD_CFLAGS([-fwrapv])

dnl Some environments abuse __STRICT_ANSI__ to disable some
dnl function declarations
PA_ADD_CPPFLAGS([-U__STRICT_ANSI__])

dnl Add -O3 unless disabled; seems to help this program quite a bit
PA_ARG_DISABLED([O3], [don't compile with -O3 optimization],
 [], [PA_ADD_CFLAGS(-O3)])

dnl Can we be selective about libraries?
PA_ADD_LDFLAGS([--as-needed])

dnl Don't put things in common if we can avoid it.  We don't want to
dnl assume all compilers support common, and this will help find those
dnl problems.  This also works around an OSX linker problem.
PA_ADD_CFLAGS([-fno-common])

dnl See if we can find sdl-config
AC_CHECK_TOOL([SDL_CONFIG], [sdl-config], [false])
AS_IF(["$SDL_CONFIG" --cflags >/dev/null 2>&1],
      [CPPFLAGS="$CPPFLAGS `"$SDL_CONFIG" --cflags`"
       LIBS="$LIBS `"$SDL_CONFIG" --libs`"],
      [AC_MSG_WARN([sdl-config not found or not functional, this is probably bad news])])

dnl Hack: on windows, build as a console app but create abc80win.exe
pa_libs_win="`echo "$LIBS" | sed -e 's/-mwindows//'`"
AS_IF([test x"$LIBS" != x"$pa_libs_win"],
      [LIBS="$pa_libs_win"
       TARGETS="$TARGETS abc80win\$(X)"
      ])

dnl SDL headers
AC_CHECK_HEADERS(SDL.h)
AC_CHECK_HEADERS(png.h)

dnl Compiler intrinsics
PA_ADD_HEADERS(intrin.h)
PA_ADD_HEADERS(x86intrin.h)
PA_HAVE_FUNC(__builtin_expect, (1,1))
PA_HAVE_FUNC(__builtin_constant_p, (0))
PA_HAVE_FUNC(__rdtsc, ())
PA_HAVE_FUNC(__umulh, (1ULL, 1ULL))
PA_HAVE_FUNC(__emulu, (1U, 1U))
PA_HAVE_FUNC(_addcarry_u64, (1, 1ULL, 1ULL, NULL))

dnl System headers
PA_ADD_HEADERS(windows.h)
AC_CHECK_HEADERS(dirent.h)
AC_CHECK_HEADERS(sys/types.h)
AC_CHECK_HEADERS(sys/stat.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(paths.h)
AC_CHECK_HEADERS(io.h)
AC_CHECK_HEADERS(direct.h)
PA_SANE_TERMIOS

dnl Useful functions which we may lack
AC_CHECK_FUNCS([mempcpy])
AC_CHECK_FUNCS([asprintf])

dnl 128-bit integers?
AC_CHECK_TYPES([uint128_t])
AC_CHECK_TYPES([__uint128_t])
AC_CHECK_TYPES([__uint128])
AC_CHECK_TYPES([unsigned __int128])

dnl Windows stat functions are a bit different, sigh...
AC_CHECK_TYPES([struct _stat64])
AC_CHECK_TYPES([struct stat])
AC_CHECK_FUNCS([stat _stat64])
AC_CHECK_FUNCS([fstat _fstat64])

dnl Newer versions of POSIX have this
AC_CHECK_TYPES(mode_t)

dnl Windows directory handling
AC_CHECK_FUNCS(_mkdir)
AC_CHECK_FUNCS([_findfirst _findfirst64])

dnl Temporary file functions
AC_CHECK_FUNCS([mkstemp tempnam])

dnl Windows file attributes
AC_CHECK_FUNCS([_setmode])

dnl How to set file size
AC_CHECK_FUNCS([ftruncate _chsize_s _chsize])

dnl Memory mapping
PA_ADD_HEADERS(sys/mman.h)
AC_CHECK_FUNCS([sysconf])
AC_CHECK_FUNCS([getpagesize])
AC_CHECK_FUNCS([mmap])
AC_CHECK_FUNCS([msync])

dnl Timekeeping...
AC_CHECK_FUNCS(clock_gettime)
AC_CHECK_FUNCS(clock_nanosleep)
AC_CHECK_FUNCS(nanosleep)
AC_CHECK_FUNCS(gettimeofday)

dnl Console handling
AC_CHECK_FUNCS(setsid)
PA_HAVE_FUNC(AllocConsole, ())
AC_FUNC_FORK

dnl Checks for library functions.
AC_SEARCH_LIBS([deflate], [z])
AC_SEARCH_LIBS([png_write_png], [png])
AC_SEARCH_LIBS([clock_gettime], [rt])

dnl
dnl Check for supported gcc attributes; some compilers (e.g. Sun CC)
dnl support these, but don't define __GNUC__ as they don't support
dnl some other features of gcc.
dnl
PA_ADD_CPPFLAGS([-Werror=attributes])
PA_FUNC_ATTRIBUTE(noreturn)
PA_FUNC_ATTRIBUTE(returns_nonnull)
PA_FUNC_ATTRIBUTE(malloc)
PA_FUNC_ATTRIBUTE(alloc_size, (1))
PA_FUNC_ATTRIBUTE(sentinel,,, [const char *, ...], ["a","b",NULL])
PA_FUNC_ATTRIBUTE(format, [(printf,1,2)], int, [const char *, ...], ["%d",1])
PA_FUNC_ATTRIBUTE(const)
PA_FUNC_ATTRIBUTE(pure)
PA_FUNC_ATTRIBUTE(cold)
PA_FUNC_ATTRIBUTE_ERROR

dnl
dnl support function sections (if available)
dnl
PA_ARG_ENABLED([sections],
 [compile with function/data section support],
 [PA_ADD_CFLAGS([-ffunction-sections])
  PA_ADD_CFLAGS([-fdata-sections])
  PA_ADD_CFLAGS([-Wl,--gc-sections])],
 [])

dnl
dnl support LTO
dnl
PA_ARG_ENABLED([lto],
 [compile with gcc-style link time optimization],
 [PA_ADD_CFLAGS([-flto])
  dnl Note: we use _PROG rather than _TOOL since we are prepending the full
  dnl CC name which ought to already contain the host triplet if needed
  ccbase=`echo "$CC" | awk '{ print $1; }'`
  AC_CHECK_PROGS(CC_AR, [${ccbase}-ar], [$ac_cv_prog_AR])
  AR="$CC_AR"
  AC_CHECK_PROGS(CC_RANLIB, [${ccbase}-ranlib], [$ac_cv_prog_RANLIB])
  RANLIB="$CC_RANLIB"], [])

dnl
dnl support sanitizers (if available)
dnl
PA_ARG_ENABLED([sanitizer],
 [compile with sanitizers enabled],
 [PA_ADD_CFLAGS([-fno-omit-frame-pointer])
  PA_ADD_CFLAGS([-fsanitize=address])
  PA_ADD_CFLAGS([-fsanitize=undefined])])

dnl
dnl Don't make symbols visible, there is no point and it just
dnl makes the code slower.
dnl
PA_ADD_CFLAGS([-fvisibility=hidden])

dnl If we have gcc, add appropriate code cleanliness options
PA_ADD_CPPFLAGS([-W])
PA_ADD_CPPFLAGS([-Wall])
PA_ADD_CPPFLAGS([-pedantic])
dnl LLVM doesn't error out on invalid -W options unless this option is
dnl specified first.  Enable this so this script can actually discover
dnl which -W options are possible for this compiler.
PA_ADD_CPPFLAGS([-Werror=unknown-warning-option])
dnl Suppress format warning on Windows targets due to their <inttypes.h>
PA_ADD_CPPFLAGS([-Wpedantic-ms-format],[-Wno-pedantic-ms-format])
dnl This is needed because we intentionally expect strncpy() to fill
dnl in a zero-padded (not zero-terminated) buffer in several backends
PA_ADD_CPPFLAGS([-Wstringop-truncation],[-Wno-stringop-truncation])
dnl PA_ADD_CPPFLAGS([-Wwrite-strings])
PA_ARG_ENABLED([werror],
 [compile with -Werror to error out on any warning],
 [PA_ADD_CPPFLAGS([-Werror])],
 [PA_ADD_CPPFLAGS([-Werror=implicit])
  PA_ADD_CPPFLAGS([-Werror=missing-braces])
  PA_ADD_CPPFLAGS([-Werror=return-type])
  PA_ADD_CPPFLAGS([-Werror=trigraphs])
  PA_ADD_CPPFLAGS([-Werror=pointer-arith])
  PA_ADD_CPPFLAGS([-Werror=strict-prototypes])
  PA_ADD_CPPFLAGS([-Werror=missing-prototypes])
  PA_ADD_CPPFLAGS([-Werror=missing-declarations])
  PA_ADD_CPPFLAGS([-Werror=comment])
  PA_ADD_CPPFLAGS([-Werror=vla])]
)

dnl
dnl On some versions of gcc, -Werror=missing-prototypes causes problems
dnl with C99-style external inlines.  Test this *after* adding the -Werror
dnl options.
dnl
PA_CHECK_BAD_STDC_INLINE

dnl
dnl support ccache
dnl
PA_ARG_ENABLED([ccache], [compile with ccache], [CC="ccache $CC"], [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
