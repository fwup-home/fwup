# Remember to run "make check" to build these. "make" and "make all" won't rebuild them.

ACLOCAL_AMFLAGS=-I m4

check_PROGRAMS=framing-helper
framing_helper_SOURCES=framing-helper.c

if HAS_VERIFY_SYSCALLS
check_PROGRAMS+=verify-syscalls
verify_syscalls_SOURCES=verify-syscalls.c
endif

if HAS_WRITE_SHIM
check_LTLIBRARIES = libwrite_shim.la
libwrite_shim_la_SOURCES = write_shim.c

# The "-rpath /nowhere" is the trick to getting libtool to create a shared library for
# "check" LTLIBRARIES.
libwrite_shim_la_LDFLAGS = ${AM_LDFLAGS} -ldl -module -avoid-version -shared -rpath /nowhere
libwrite_shim_la_CFLAGS = ${AM_CFLAGS}
endif
