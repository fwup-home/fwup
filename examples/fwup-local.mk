FWUP_OVERRIDE_SRCDIR = /home/fhunleth/experiments/fwup

# Hack the call to rsync so that we can reference the checked out version
# of fwup without recursively copying the examples directory.
override RSYNC_VCS_EXCLUSIONS := --exclude examples --exclude build \
    --exclude .git --exclude '*.o' --exclude Makefile --exclude Makefile.in \
    --exclude configure

