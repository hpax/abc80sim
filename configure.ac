dnl Process this file with autoconf 2.69 or later to produce
dnl a configure script.
AC_PREREQ([2.71])

AC_CONFIG_MACRO_DIR(autoconf)
AC_CONFIG_MACRO_DIRS(autoconf)

AC_INIT
AC_CONFIG_SRCDIR([abc80.c])
AC_CONFIG_HEADERS(config/config.h)

AC_CONFIG_AUX_DIR(autoconf/helpers)

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
PA_PROG_CC
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_INSTALL
PA_CC_AUTODEPS

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_SIZE_T

AC_HEADER_STDBOOL
AC_CHECK_HEADERS(inttypes.h)
AC_CHECK_HEADERS(stdnoreturn.h)
AC_CHECK_HEADERS(stdatomic.h)

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
PA_ADD_LDFLAGS([-Wl,--as-needed])

dnl Don't put things in common if we can avoid it.  We don't want to
dnl assume all compilers support common, and this will help find those
dnl problems.  This also works around an OSX linker problem.
PA_ADD_CFLAGS([-fno-common])

dnl gcc stack protector; also needed for MinGW to build dirent.h correctly...
PA_ADD_CFLAGS([-fstack-protector])

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
AC_CHECK_HEADERS(synchapi.h)
AC_CHECK_HEADERS(mmsystem.h)
AC_CHECK_HEADERS(timeapi.h)
AC_CHECK_HEADERS(process.h)

AC_CHECK_TYPES(ssize_t)
AC_CHECK_TYPES(ptrdiff_t)

dnl Sockets
PA_ADD_HEADERS(netdb.h)
PA_ADD_HEADERS(winsock2.h)
PA_ADD_HEADERS(sys/socket.h)
PA_ADD_HEADERS(ws2tcpip.h)
PA_ADD_HEADERS(netinet/in.h)
PA_ADD_HEADERS(netinet/ip.h)
PA_ADD_HEADERS(netinet/tcp.h)
PA_ADD_HEADERS(arpa/inet.h)
PA_ADD_HEADERS(afunix.h)
PA_ADD_HEADERS(sys/un.h)
PA_ADD_HEADERS(net/ethernet.h)
PA_ADD_HEADERS(linux/if_packet.h)
PA_ADD_HEADERS(linux/netlink.h)
AC_CHECK_TYPES(socklen_t)
AC_CHECK_TYPES(sa_family_t)
AC_CHECK_TYPES(struct sockaddr_in)
AC_CHECK_TYPES(struct sockaddr_in6)
AC_CHECK_TYPES(struct sockaddr_un)
AC_CHECK_TYPES(struct sockaddr_ll)
AC_CHECK_TYPES(struct sockaddr_nl)
AC_CHECK_TYPES(struct sockaddr_storage)
AC_CHECK_TYPES(struct addrinfo)
AC_CHECK_FUNCS(getaddrinfo)
AC_CHECK_FUNCS(accept4)

dnl termios and speed_t
PA_ADD_HEADERS(termios.h)
AC_CHECK_TYPES(speed_t)

dnl Useful functions which we may lack
AC_CHECK_FUNCS([mempcpy])
AC_CHECK_FUNCS([asprintf])

dnl Windows stat functions are a bit different, sigh...
AC_CHECK_TYPES([struct _stat64])
AC_CHECK_TYPES([struct stat])
AC_CHECK_FUNCS([stat _stat64])
AC_CHECK_FUNCS([fstat _fstat64])

dnl Other things different in Windows...
PA_ADD_HEADERS(time.h)
AC_SEARCH_LIBS([strcpy_s], [ssp])
AC_SEARCH_LIBS([__strcpy_chk], [ssp])
AC_CHECK_FUNCS(localtime_r)
PA_HAVE_FUNC([localtime_s], [(NULL, NULL)])

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

dnl Process and console handling
AC_FUNC_FORK
AC_CHECK_FUNCS([getpid _getpid GetCurrentProcessId])
AC_CHECK_FUNCS(setsid)
PA_HAVE_FUNC(AllocConsole, ())

dnl Checks for library functions.
AC_SEARCH_LIBS([deflate], [z])
AC_SEARCH_LIBS([png_write_png], [png])
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_SEARCH_LIBS([timeBeginPeriod], [winmm])
AC_SEARCH_LIBS([WSAStartup], [ws2_32])

dnl
dnl Some rather useful gcc extensions...
dnl
PA_HAVE_FUNC(__builtin_constant_p, (0))
PA_HAVE_FUNC(__builtin_choose_expr, (0,1,2))

dnl
dnl Check for supported gcc attributes; some compilers (e.g. Sun CC)
dnl support these, but don't define __GNUC__ as they don't support
dnl some other features of gcc.
dnl
PA_ADD_CFLAGS([-Werror=attributes])
PA_FUNC_ATTRIBUTE(noreturn)
PA_FUNC_ATTRIBUTE(returns_nonnull,,,,,never_null)
PA_FUNC_ATTRIBUTE(malloc)
PA_FUNC_ATTRIBUTE(alloc_size,[1])
PA_FUNC_ATTRIBUTE(alloc_size,[1,2])
PA_FUNC_ATTRIBUTE(sentinel,,, [const char *, ...], ["a","b",NULL],end_with_null)
PA_FUNC_ATTRIBUTE(format, [printf,1,2], int, [const char *, ...], ["%d",1])
PA_FUNC_ATTRIBUTE(const)
PA_FUNC_ATTRIBUTE(pure)
PA_FUNC_ATTRIBUTE(cold,,,,,unlikely_func)
PA_FUNC_ATTRIBUTE(unused)
PA_FUNC_ATTRIBUTE_ERROR

dnl
dnl Support function sections (if available)
dnl Would be nice to know how to do this on other compilers...
dnl
PA_ARG_DISABLED([gc],
 [don't compile with dead code elimination],
 [],
 [PA_ADD_CFLAGS([-ffunction-sections])
  PA_ADD_CFLAGS([-fdata-sections])
  PA_ADD_CFLAGS([-Wl,--gc-sections])])

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

dnl Various warnings...
PA_ADD_CPPFLAGS([-Wimplicit])
PA_ADD_CPPFLAGS([-Wmissing-braces])
PA_ADD_CPPFLAGS([-Wreturn-type])
PA_ADD_CPPFLAGS([-Wtrigraphs])
PA_ADD_CPPFLAGS([-Wpointer-arith])
PA_ADD_CPPFLAGS([-Wstrict-prototypes])
PA_ADD_CPPFLAGS([-Wmissing-prototypes])
PA_ADD_CPPFLAGS([-Wmissing-declarations])
PA_ADD_CPPFLAGS([-Wcomment])
PA_ADD_CPPFLAGS([-Wvla])]
PA_ARG_ENABLED([werror],
 [compile with -Werror to error out on any warning],
 [PA_ADD_CPPFLAGS([-Werror])],
)

dnl Warnings that are probabilistic based on the compiler version, and
dnl only should be used specifically when looking for opportunities to
dnl address or optimize these cases.
PA_ARG_ENABLED([suggestions],
 [compile with compiler suggestion warnings enabled],
 [PA_ADD_CFLAGS([-Wsuggest-attribute=pure])
  PA_ADD_CFLAGS([-Wsuggest-attribute=const])
  PA_ADD_CFLAGS([-Wsuggest-attribute=noreturn])
  PA_ADD_CFLAGS([-Wsuggest-attribute=format])
  PA_ADD_CFLAGS([-Wsuggest-attribute=cold])
  PA_ADD_CFLAGS([-Wsuggest-attribute=malloc])])

dnl
dnl On some versions of gcc, -Werror=missing-prototypes causes problems
dnl with C99-style external inlines.  Test this *after* adding the -Werror
dnl options.
dnl
PA_CHECK_BAD_STDC_INLINE
PA_C_TYPEOF

dnl
dnl support ccache
dnl
PA_ARG_ENABLED([ccache], [compile with ccache], [CC="ccache $CC"], [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
