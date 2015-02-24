AC_INIT([fwup], [0.1.1], [fhunleth@troodon-software.com])
AC_CONFIG_SRCDIR([src/fwup_metadata.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h malloc.h stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_SYS_LARGEFILE

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset setenv strdup strtoul])

AC_CHECK_LIB([confuse], [cfg_init], [], [ AC_MSG_ERROR([Could not find libconfuse]) ])
AC_CHECK_LIB([archive], [archive_read_new], [], [ AC_MSG_ERROR([Could not find libarchive]) ])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
