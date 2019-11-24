#!/bin/bash

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

set -e

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

source $BASE_DIR/scripts/common.sh

# Initial sanity checks
if [ ! -e $BASE_DIR/configure ]; then
    echo "Please run from the fwup base directory and make sure that the ./configure file exists."
    echo "If you're building from source, you may need to run ./autogen.sh."
    exit 1
fi

if [ -e $BASE_DIR/Makefile ]; then
    # Run distclean to ensure that we have a clean build.
    make distclean
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
    make $MAKE_FLAGS check
fi

make install-strip
make dist

# Return to the base directory
cd $BASE_DIR

echo "Static build successful."
echo "The fwup installation is in $FWUP_INSTALL_DIR."

