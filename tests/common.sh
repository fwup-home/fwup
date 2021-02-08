#!/bin/sh

# Common unit test commands
set -e

export LC_ALL=C

# Linux command line tools that may be different on other OSes
READLINK=readlink
BASE64_DECODE=-d
FSCK_FAT=fsck.fat
TIMEOUT=timeout

if [ -d "/mnt/c/Users" ]; then
    # Windows 10 bash mode
    HOST_OS=Windows
    HOST_ARCH=amd64
else
    HOST_OS=$(uname -s)
    HOST_ARCH=$(uname -m)
fi

case "$HOST_OS" in
    Darwin)
	# BSD stat
        STAT_FILESIZE_FLAGS="-f %z"

	# Not -d?
        BASE64_DECODE=-D

        BREW_PREFIX=$(brew --prefix)
        READLINK="$BREW_PREFIX"/bin/greadlink
        [ -e "$READLINK" ] || ( echo "Please run 'brew install coreutils' to install greadlink"; exit 1 )
        [ -e "$BREW_PREFIX/bin/mdir" ] || ( echo "Please run 'brew install mtools' to install mdir"; exit 1 )

        FSCK_FAT=fsck_msdos
        TIMEOUT=gtimeout
        ;;
    FreeBSD|NetBSD|OpenBSD|DragonFly)
	# BSD stat
        STAT_FILESIZE_FLAGS="-f %z"

        # fsck_msdosfs fails on BSD, but the failure doesn't look like a
        # problem, so ignore.
        #FSCK_FAT=fsck_msdosfs
        FSCK_FAT=true

        # The BSDs don't have the timeout command
        TIMEOUT=
        ;;
    *)
	# GNU stat
	STAT_FILESIZE_FLAGS=-c%s

        # Check for Busybox timeout which is a symlink
	if [ -L $(which $TIMEOUT) ]; then
		TIMEOUT=
	fi
        ;;
esac

base64_decode() {
    base64 $BASE64_DECODE
}

base64_decodez() {
    base64 $BASE64_DECODE | zcat
}

filesize() {
    stat $STAT_FILESIZE_FLAGS $1
}

TESTS_SRC_DIR=$(dirname $($READLINK -f $0))
TESTS_DIR=$(pwd)

# Default to testing the fwup built in the src directory,
# but it is possible to define the version of fwup used for
# the create and apply steps separately.
FWUP_DEFAULT=$TESTS_DIR/../src/fwup
FWUP_BINARY=$TESTS_DIR/../src/fwup
FRAMING_HELPER=$TESTS_DIR/fixture/framing-helper
if [ ! -e $FWUP_DEFAULT ]; then
    if [ -e $FWUP_DEFAULT.exe ]; then
        EXEEXT=.exe
        FWUP_BINARY="$FWUP_DEFAULT.exe"
        FWUP_DEFAULT="wine $FWUP_BINARY"
        FRAMING_HELPER="wine $TESTS_DIR/fixture/framing-helper.exe"
    fi
fi

if [ -z $FWUP_CREATE ]; then FWUP_CREATE="$FWUP_DEFAULT"; fi
if [ -z $FWUP_APPLY ]; then FWUP_APPLY="$FWUP_DEFAULT"; fi
if [ -z $FWUP_APPLY_NO_CHECK ]; then FWUP_APPLY_NO_CHECK="$FWUP_APPLY"; fi
if [ -z $FWUP_VERIFY ]; then FWUP_VERIFY="$FWUP_DEFAULT"; fi

if [ -z $EXEEXT ]; then
    # No sanity check for windows builds.
    [ -e $FWUP_CREATE ] || ( echo "Can't find $FWUP_CREATE"; exit 1 )
    [ -e $FWUP_APPLY ] || ( echo "Can't find $FWUP_APPLY"; exit 1 )
    [ -e $FWUP_VERIFY ] || ( echo "Can't find $FWUP_VERIFY"; exit 1 )
    [ -e $FRAMING_HELPER ] || ( echo "Can't find $FRAMING_HELPER"; exit 1 )
fi

if [ -n "$VALGRIND" ]; then
    # Example:
    #  VALGRIND="valgrind -s --leak-check=full --error-exitcode=1 --track-origins=yes
    FWUP_CREATE="$VALGRIND $FWUP_CREATE"
    FWUP_APPLY="$VALGRIND $FWUP_APPLY"
    FWUP_VERIFY="$VALGRIND $FWUP_VERIFY"
else
# The syscall verification code only runs on a subset of
# platforms. Let autoconf figure out which ones and run it
# if the verify-syscalls program built.
if [ -e $TESTS_DIR/fixture/verify-syscalls ]; then
    export VERIFY_SYSCALLS_CMD=$FWUP_APPLY
    FWUP_APPLY=$TESTS_DIR/fixture/verify-syscalls
fi
fi

# The write fault simulator only runs only runs on a subset of
# platforms. Let autoconf figure out which ones.
WRITE_SHIM="$TESTS_DIR/fixture/.libs/libwrite_shim.so"
if [ -e "$WRITE_SHIM" ]; then
    export HAS_WRITE_SHIM=true
    export LD_PRELOAD="$WRITE_SHIM"
    export DYLD_INSERT_LIBRARIES="$WRITE_SHIM"
else
    export HAS_WRITE_SHIM=false
fi

WORK=$TESTS_DIR/work-$(basename "$0")
RESULTS=$WORK/results

CONFIG=$WORK/fwup.conf
FWFILE=$WORK/fwup.fw
IMGFILE=$WORK/fwup.img
UNZIPDIR=$WORK/unzip
EXPECTED_META_CONF=$WORK/meta.conf.expected
TRIMMED_META_CONF=$WORK/meta.conf.trimmed

# Setup the directories
rm -fr $WORK
mkdir -p $WORK

unzip_fw() {
    mkdir -p $UNZIPDIR
    unzip -q $FWFILE -d $UNZIPDIR
}

check_meta_conf() {
    # Need to unzip the fw file to check the meta.conf
    if ! [ -e $UNZIPDIR/meta.conf ]; then
        unzip_fw
    fi

    # Trim the results of known lines that vary between runs
    cat $UNZIPDIR/meta.conf \
        > $TRIMMED_META_CONF
    diff -w $EXPECTED_META_CONF $TRIMMED_META_CONF
}

cleanup() {
    rc=$?
    if [ $rc = "0" ]; then
        echo "Test succeeded"
        rm -fr $WORK
    elif [ $rc = "77" ]; then
        echo "Test skipped"
        rm -fr $WORK
    else
        echo
        echo "Test failed!"
        echo
        echo "Leaving test work files in '$WORK'"
    fi
}

trap cleanup EXIT

# Test input files

# These files contain random data so that it is possible to
# verify that fwup copied things correctly. Previously fwup
# unit tests used files filled with 1s, and it possible for
# a test to pass even though the data had been reordered.
# (This never actually happened to my knowledge.)

TESTFILE_1K="$TESTS_SRC_DIR/1K.bin"
TESTFILE_1K_CORRUPT="$TESTS_SRC_DIR/1K-corrupt.bin"
TESTFILE_150K="$TESTS_SRC_DIR/150K.bin"

# Generated test data
create_15M_file() {
    if [ ! -e $TESTFILE_15M ]; then
        i=0
        while [ $i -lt 100 ]; do
            cat $TESTFILE_150K >> $TESTFILE_15M
            i=$(expr $i + 1)
        done
    fi
}
TESTFILE_15M=$WORK/15M.bin

# GNU and BSD friendly cmp --bytes
# Invoke as: cmp_bytes <byte count> <file1> <file2> [offset1] [offset2]
#
# NOTE: This implementation has the unfortunate constraint that bytes and
#       offsets need to be multiples of the block size. The block size
#       could be set to 1, but that makes some large comparisons VERY slow.
cmp_bytes() {
    BYTE_COUNT=$1
    FILE1=$2
    FILE2=$3
    BEGIN1=$4
    BEGIN2=$5

    if [ -z $BEGIN1 ]; then BEGIN1=0; fi
    if [ -z $BEGIN2 ]; then BEGIN2=0; fi

    BEGIN1=$(expr $BEGIN1 / 512) || true
    BEGIN2=$(expr $BEGIN2 / 512) || true
    BLOCK_COUNT=$(expr $BYTE_COUNT / 512)

    dd if=$FILE1 of=$WORK/cmp_bytes1 count=$BLOCK_COUNT skip=$BEGIN1 2> /dev/null
    dd if=$FILE2 of=$WORK/cmp_bytes2 count=$BLOCK_COUNT skip=$BEGIN2 2> /dev/null

    cmp $WORK/cmp_bytes1 $WORK/cmp_bytes2
}
