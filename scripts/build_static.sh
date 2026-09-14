#!/usr/bin/env bash

#
# Build a static (or mostly static) version of fwup
#
# Inputs:
#     CROSS_COMPILE - if set to a gcc tuple, tries to crosscompile
#                     (e.g., x86_64-w64-mingw32)
#
# This script creates a static build of fwup to avoid dependency issues
# with libconfuse and libarchive.
#
# To build the Windows executable on Linux:
#  sudo apt-get install gcc-mingw-w64-x86-64
#  CROSS_COMPILE=x86_64-w64-mingw32 ./scripts/build_pkg.sh
#

set -eo pipefail

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

source $BASE_DIR/scripts/common.sh

FWUP_VERSION=$(cat "$BASE_DIR/VERSION")

# Initial sanity checks
if [ ! -e $BASE_DIR/configure ]; then
    echo "Please run from the fwup base directory and make sure that the ./configure file exists."
    echo "If you're building from source, you may need to run ./autogen.sh."
    exit 1
fi

if [ -e $BASE_DIR/Makefile ]; then
    # Run distclean to ensure that we have a clean build.
    make -C $BASE_DIR distclean
fi

# Build the dependencies
$BASE_DIR/scripts/build_deps.sh

# Initialize some directories
mkdir -p $BUILD_DIR
mkdir -p $FWUP_INSTALL_DIR

cd $BUILD_DIR

# Build fwup (symlink now, since out-of-tree fwup build is broke)
ln -sf $BASE_DIR $BUILD_DIR/fwup
cd $BUILD_DIR/fwup
PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --prefix=$FWUP_INSTALL_DIR --enable-shared=no || cat config.log
make clean
make $MAKE_FLAGS

if [ -z "$CROSS_COMPILE" ]; then
    # Verify that it was statically linked
    for CHECK_LIB in libz confuse archive; do
        if $LDD src/fwup | grep $CHECK_LIB; then
            echo "fwup was dynamically linked to $CHECK_LIB. This should not happen.";
            exit 1
        fi
    done
fi

if [ "$CROSS_COMPILE" != "arm-linux-gnueabihf" ]; then
    # Run the regression tests
    if [ "$MODE" = "windows" ]; then
        # Make sure that wine is initialized by running it once.
        wine src/fwup.exe || true
    fi

    if ! make $MAKE_FLAGS check; then
        cat tests/test-suite.log
        exit 1
    fi
fi

make install-strip
make dist

if [ "$(uname -s)" = "Linux" ] && [ -n "$SOURCE_DATE_EPOCH" ]; then
    DIST_ARCHIVE="$BASE_DIR/fwup-$FWUP_VERSION.tar.gz"
    DIST_REPACK_DIR="$BUILD_DIR/fwup-dist-repack"
    rm -rf "$DIST_REPACK_DIR"
    mkdir -p "$DIST_REPACK_DIR"
    tar -xzf "$DIST_ARCHIVE" -C "$DIST_REPACK_DIR"
    find "$DIST_REPACK_DIR" -exec touch -h --date="@$SOURCE_DATE_EPOCH" {} +
    tar --sort=name \
        --format=ustar \
        --mtime="@$SOURCE_DATE_EPOCH" \
        --owner=0 \
        --group=0 \
        --numeric-owner \
        --mode='go=rX,u+rw,a-s' \
        -C "$DIST_REPACK_DIR" \
        -cf - "fwup-$FWUP_VERSION" |
        gzip -9 -n > "$DIST_ARCHIVE.reproducible"
    mv "$DIST_ARCHIVE.reproducible" "$DIST_ARCHIVE"
    rm -rf "$DIST_REPACK_DIR"
fi

# Return to the base directory
cd $BASE_DIR

echo "Static build successful."
echo "The fwup installation is in $FWUP_INSTALL_DIR."
