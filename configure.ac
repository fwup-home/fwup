AC_INIT([fwup], m4_esyscmd_s(cat VERSION), [fhunleth@troodon-software.com])
AC_CONFIG_SRCDIR([src/fwup_metadata.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

# Which platform?
AC_CANONICAL_HOST

# Check for programs
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PATH_PROG([HELP2MAN], help2man)
AM_CONDITIONAL([HAVE_HELP2MAN], [test -n "$HELP2MAN"])

# Check for header files
AC_CHECK_HEADERS([fcntl.h inttypes.h malloc.h stddef.h stdint.h \
                  stdlib.h string.h unistd.h archive.h confuse.h \
                  sys/ptrace.h])

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

# Check for library functions
AC_CHECK_FUNCS([memset gettimeofday setenv strdup strndup \
                strtoul umount fcntl strptime setenv pread \
                pwrite memmem ptrace posix_memalign sysconf])
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

PKG_CHECK_MODULES([ARCHIVE], [libarchive], [], AC_MSG_ERROR([Requires libarchive. Libarchive must be built with zlib supprot.]))
PKG_CHECK_MODULES([CONFUSE], [libconfuse >= 2.8], [], AC_MSG_ERROR([Requires libconfuse v2.8 or later]))
PKG_CHECK_MODULES([SODIUM], [libsodium], [], AC_MSG_ERROR([Requires libsodium]))

# Pthreads are optional
AX_PTHREAD()

# Platform-specific additions
AS_CASE([$host_os],
        [darwin*], [LIBS="$LIBS -framework CoreFoundation -framework DiskArbitration"]
        )

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

# Ptrace checks in regression tests
AM_CONDITIONAL([IS_CROSSCOMPILING], [test x$cross_compiling = x"yes"])
AM_CONDITIONAL([HAS_VERIFY_SYSCALLS], [test x$host = x"x86_64-pc-linux-gnu"])

# Coverage tests
AC_ARG_ENABLE(gcov,[AS_HELP_STRING([--enable-gcov], [enable coverage test])])
AC_MSG_CHECKING([whether to enable gcov])
AS_IF([test "x${enable_gcov}" = "xyes" ], AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))
AM_CONDITIONAL([ENABLE_GCOV],[test "x${enable_gcov}" = "xyes"])

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile docs/Makefile])

AC_OUTPUT
