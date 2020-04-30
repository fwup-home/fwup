ACLOCAL_AMFLAGS=-I m4

noinst_PROGRAMS=framing-helper
framing_helper_SOURCES=framing-helper.c

if HAS_VERIFY_SYSCALLS

# Even if verify-syscalls builds, don't use it when crosscompiling
# yet until confident that syscalls are never altered when going
# through qemu. Also, Windows doesn't have the same constraints
# as Linux, so verify-syscalls may be overly picky.
if !IS_CROSSCOMPILING
noinst_PROGRAMS+=verify-syscalls
verify_syscalls_SOURCES=verify-syscalls.c
endif
endif

