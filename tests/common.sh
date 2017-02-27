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

        READLINK=/usr/local/bin/greadlink
        [ -e $READLINK ] || ( echo "Please run 'brew install coreutils' to install greadlink"; exit 1 )
        [ -e /usr/local/bin/mdir ] || ( echo "Please run 'brew install mtools' to install mdir"; exit 1 )

        FSCK_FAT=fsck_msdos
        TIMEOUT=gtimeout
        ;;
    FreeBSD|NetBSD|OpenBSD|DragonFly)
	# BSD stat
        STAT_FILESIZE_FLAGS="-f %z"
        ;;
    *)
	# GNU stat
	STAT_FILESIZE_FLAGS=-c%s
        ;;
esac

base64_decode() {
    base64 $BASE64_DECODE
}

filesize() {
    stat $STAT_FILESIZE_FLAGS $1
}

TESTS_DIR=$(dirname $($READLINK -f $0))

# Default to testing the fwup built in the src directory,
# but it is possible to define the version of fwup used for
# the create and apply steps separately.
FWUP_DEFAULT=$TESTS_DIR/../src/fwup
if [ ! -e $FWUP_DEFAULT ]; then
    if [ -e $FWUP_DEFAULT.exe ]; then
        EXEEXT=.exe
        FWUP_DEFAULT=$FWUP_DEFAULT.exe
    fi
fi

if [ -z $FWUP_CREATE ]; then FWUP_CREATE=$FWUP_DEFAULT; fi
if [ -z $FWUP_APPLY ]; then FWUP_APPLY=$FWUP_DEFAULT; fi
if [ -z $FWUP_APPLY_NO_CHECK ]; then FWUP_APPLY_NO_CHECK=$FWUP_APPLY; fi
if [ -z $FWUP_VERIFY ]; then FWUP_VERIFY=$FWUP_DEFAULT; fi

FRAMING_HELPER=$TESTS_DIR/framing-helper$EXEEXT

# The syscall verification code only runs on a subset of
# platforms. Let autoconf figure out which ones and run it
# if the verify-syscalls program built.
if [ -e $TESTS_DIR/verify-syscalls ]; then
    export VERIFY_SYSCALLS_CMD=$FWUP_APPLY
    FWUP_APPLY=$TESTS_DIR/verify-syscalls
fi

WORK=$TESTS_DIR/work-$(basename "$0")
RESULTS=$WORK/results

[ -e $FWUP_CREATE ] || ( echo "Can't find $FWUP_CREATE"; exit 1 )
[ -e $FWUP_APPLY ] || ( echo "Can't find $FWUP_APPLY"; exit 1 )
[ -e $FWUP_VERIFY ] || ( echo "Can't find $FWUP_VERIFY"; exit 1 )
[ -e $FRAMING_HELPER ] || ( echo "Can't find $FRAMING_HELPER"; exit 1 )

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
    cat $UNZIPDIR/meta.conf | \
        grep -v "^meta-creation-date" | \
        grep -v "^meta-fwup-version" \
        > $TRIMMED_META_CONF
    diff -w $EXPECTED_META_CONF $TRIMMED_META_CONF
}

cleanup() {
    if [ $? = "0" ]; then
        echo "Test succeeded"
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

TESTFILE_1K=$TESTS_DIR/1K.bin
TESTFILE_1K_CORRUPT=$TESTS_DIR/1K-corrupt.bin
TESTFILE_150K=$TESTS_DIR/150K.bin

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
