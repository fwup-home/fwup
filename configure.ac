AC_INIT([fwup], [0.8.2], [fhunleth@troodon-software.com])
AC_CONFIG_SRCDIR([src/fwup_metadata.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

# Which platform?
AC_CANONICAL_HOST

# Checks for programs
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PATH_PROG([HELP2MAN], help2man)
AM_CONDITIONAL([HAVE_HELP2MAN], [test -n "$HELP2MAN"])

# Checks for header files
AC_CHECK_HEADERS([fcntl.h inttypes.h malloc.h stddef.h stdint.h stdlib.h string.h unistd.h archive.h confuse.h])

# Checks for typedefs, structures, and compiler characteristics
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

# Checks for library functions
AC_CHECK_FUNCS([memset setenv strdup strtoul umount fcntl strptime setenv pread pwrite])
AM_CONDITIONAL([HAS_STRPTIME], [test x$ac_vc_func_strptime = x"yes"])

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

# Coverage tests
AC_ARG_ENABLE(gcov,[AS_HELP_STRING([--enable-gcov], [enable coverage test])])
AC_MSG_CHECKING([whether to enable gcov])
AS_IF([test "x${enable_gcov}" = "xyes" ], AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))
AM_CONDITIONAL([ENABLE_GCOV],[test "x${enable_gcov}" = "xyes"])

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile docs/Makefile])

AC_OUTPUT
