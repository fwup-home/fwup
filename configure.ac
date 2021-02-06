AC_INIT([fwup], m4_esyscmd_s(cat VERSION), [https://github.com/fwup-home/fwup/issues])
AC_CONFIG_SRCDIR([src/fwup_metadata.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

# Which platform?
AC_CANONICAL_HOST

# Check for programs
AC_PROG_CC
AM_PROG_AR
AC_PROG_LIBTOOL
AC_PATH_PROG([HELP2MAN], help2man)
AM_CONDITIONAL([HAVE_HELP2MAN], [test -n "$HELP2MAN"])

# Check for header files
AC_CHECK_HEADERS([fcntl.h inttypes.h malloc.h stddef.h stdint.h \
                  stdlib.h string.h unistd.h archive.h confuse.h \
                  sys/ptrace.h libgen.h sys/random.h])

# Check for typedefs, structures, and compiler characteristics
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_SYS_LARGEFILE
AC_PROG_CC_C99

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(unsigned long long)

# Check for library functions
AC_CHECK_FUNCS([memset gettimeofday setenv strdup strndup \
                strtoul umount fcntl strptime setenv pread \
                pwrite memmem ptrace posix_memalign sysconf \
                clock_gettime dirname timegm])
AM_CONDITIONAL([HAS_STRPTIME], [test x$ac_cv_func_strptime = x"yes"])
AM_CONDITIONAL([HAS_PTRACE], [test x$ac_cv_func_ptrace = x"yes"])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
                   [[
                    #define _GNU_SOURCE
                    #include <sys/types.h>
                    #include <unistd.h>
                    ]],
                    [[
                     int x = SEEK_HOLE+SEEK_DATA;
                     ]])],
                  [AC_DEFINE([HAVE_SPARSE_SEEK], [1], [Defined if sparse seeking is supported])
                   have_sparse_seek="yes"],
                  [AC_MSG_WARN([Sparse seek support not found. fwup won't be able to detect holes in sparse file resources])])
AM_CONDITIONAL([HAVE_SPARSE_SEEK], [test "${have_sparse_seek}" = "yes"])

AC_PATH_PROG([PKG_CONFIG], [pkg-config], [no])
AS_IF([test "x$PKG_CONFIG" = "xno"],[
   AC_MSG_ERROR([
      *** The pkg-config script could not be found. Make sure it is
      *** in your path, or set the PKG_CONFIG environment variable
      *** to the full path to pkg-config.])
   ])

PKG_CHECK_MODULES([ARCHIVE], [libarchive], [], AC_MSG_ERROR([Requires libarchive. Libarchive must be built with zlib support.]))
PKG_CHECK_MODULES([CONFUSE], [libconfuse >= 2.8], [], AC_MSG_ERROR([Requires libconfuse v2.8 or later]))

AC_ARG_WITH([pthreads],
            [AS_HELP_STRING([--with-pthreads],
                            [Use pthreads to improve performance especially on slower systems. @<:@default=yes@:>@])],
                            [],
                            [with_pthreads=yes])
AS_CASE(["$with_pthreads"],
        [yes], [AX_PTHREAD()],
        [no], [],
        [AX_PTHREAD()])

# Platform-specific additions
AS_CASE([$host_os],
        [darwin*], [LIBS="$LIBS -framework CoreFoundation -framework DiskArbitration"]
        )

AC_SEARCH_LIBS([BCryptGenRandom], [bcrypt], [], [])

# Limit functionality for minimal size
AC_ARG_ENABLE(minimal-build,[AS_HELP_STRING([--enable-minimal-build], [create minimal build (only supports applying updates)])])
AS_IF([test "x${enable_minimal_build}" = "xyes" ], [AC_DEFINE([FWUP_MINIMAL], [1], [Defined if functionality should be limited to applying updates])])

# Shell completion
AC_ARG_WITH([bash-completion-dir],
    AS_HELP_STRING([--with-bash-completion-dir[=PATH]],
        [Install the bash auto-completion script in this directory. @<:@default=yes@:>@]),
    [],
    [with_bash_completion_dir=yes])

if test "x$with_bash_completion_dir" = "xyes"; then
    # Hardcode installation to the normal location. Using PKG_CHECK_MODULES
    # to determine whether to use the old location in /etc/bash_completion.d/
    # turned out to be unreliable.
    BASH_COMPLETION_DIR="$datadir/bash-completion/completions"
else
    BASH_COMPLETION_DIR="$with_bash_completion_dir"
fi

AC_SUBST([BASH_COMPLETION_DIR])
AM_CONDITIONAL([ENABLE_BASH_COMPLETION],[test "x$with_bash_completion_dir" != "xno"])

# Test for whether it's possible to build test fixtures
#
# verify-syscalls - requires ptrace so Linux-only
# write_shim - shared library. Hasn't been ported to OSX.
AM_CONDITIONAL([HAS_VERIFY_SYSCALLS], [test \( x$host = x"x86_64-pc-linux-gnu" -o x$host = x"x86_64-unknown-linux-gnu" \) -a x$cross_compiling = x"no"])
AM_CONDITIONAL([HAS_WRITE_SHIM], [test \( x$host = x"x86_64-pc-linux-gnu" -o x$host = x"x86_64-unknown-linux-gnu" \) -a x$cross_compiling = x"no" -a x$enable_shared = x"yes"])

# Scripts
AC_ARG_ENABLE(scripts,[AS_HELP_STRING([--disable-scripts], [do not install img2fwup and any other helper scripts])])
AC_MSG_CHECKING([whether to install scripts])
AS_IF([test "x${enable_scripts}" != "xno" ], AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))
AM_CONDITIONAL([ENABLE_SCRIPTS],[test "x${enable_scripts}" != "xno"])

# Coverage tests
AC_ARG_ENABLE(gcov,[AS_HELP_STRING([--enable-gcov], [enable coverage test])])
AC_MSG_CHECKING([whether to enable gcov])
AS_IF([test "x${enable_gcov}" = "xyes" ], AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))
AM_CONDITIONAL([ENABLE_GCOV],[test "x${enable_gcov}" = "xyes"])

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile docs/Makefile tests/fixture/Makefile])

AC_OUTPUT
