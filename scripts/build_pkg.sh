#!/bin/bash

#
# Package fwup
#
# Inputs:
#     CROSS_COMPILE - if set to a gcc tuple, tries to crosscompile
#                     (e.g., x86_64-w64-mingw32)
#
# This script creates a self-contained .deb and .rpm that should
#  work on any Linux (assuming it's running on the
# same processor architecture) or an .exe for Windows.
#
set -e

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

source $BASE_DIR/scripts/common.sh

# Check if the static build was run
if [ ! -d $FWUP_INSTALL_DIR ]; then
    $BASE_DIR/scripts/build_static.sh
fi

if [ $(uname -s) = "Darwin" ]; then
    echo "Packaging not supported on OSX"
    exit 0
fi

# Package fwup
FWUP_VERSION=$(cat $BASE_DIR/VERSION)
if [ -z "$CROSS_COMPILE" ]; then
    # Fix directory permissions for packaging
    find $FWUP_STAGING_DIR -type d | xargs chmod 755

    # Debian requires compressed man pages
    # NOTE: Even though building man pages is optional, it is
    #       an error if man pages don't exist when creating packages
    gzip -9 -f $FWUP_STAGING_DIR/usr/share/man/man1/fwup.1

    # Build Linux packages
    rm -f fwup_*.deb fwup-*.rpm
    fpm -s dir -t deb -v $FWUP_VERSION -n fwup -m "$FWUP_MAINTAINER" \
        --license "$FWUP_LICENSE" --description "$FWUP_DESCRIPTION" \
        --depends "libc6 (>= 2.4)" --deb-priority optional --category devel \
        --url "$FWUP_HOMEPAGE" --vendor "$FWUP_VENDOR" -C $FWUP_STAGING_DIR

    fpm -s dir -t rpm -v $FWUP_VERSION -n fwup -m "$FWUP_MAINTAINER" \
        --license "$FWUP_LICENSE" --description "$FWUP_DESCRIPTION" \
        --url "$FWUP_HOMEPAGE" --vendor "$FWUP_VENDOR" -C $FWUP_STAGING_DIR
elif [ "$CROSS_COMPILE" = "x86_64-w64-mingw32" ]; then
    # Build Windows package
    rm -f fwup.exe
    cp $FWUP_INSTALL_DIR/bin/fwup.exe .

    mkdir -p $FWUP_INSTALL_DIR/fwup/tools
    cp -f scripts/fwup.nuspec $FWUP_INSTALL_DIR/fwup/
    sed -i "s/%VERSION%/$FWUP_VERSION/" $FWUP_INSTALL_DIR/fwup/fwup.nuspec
    cp $FWUP_INSTALL_DIR/bin/fwup.exe $FWUP_INSTALL_DIR/fwup/tools/

    cp -f scripts/VERIFICATION.txt $FWUP_INSTALL_DIR/fwup/tools/
    sed -i "s/%VERSION%/$FWUP_VERSION/" $FWUP_INSTALL_DIR/fwup/tools/VERIFICATION.txt
    cat scripts/LICENSE.txt LICENSE > $FWUP_INSTALL_DIR/fwup/tools/LICENSE.txt

    # Wait to the last minute to build and install chocolatey since it's
    # such a pain and kills Travis builds randomly and frequently.
    $BASE_DIR/scripts/ubuntu_install_chocolatey.sh

    cd $FWUP_INSTALL_DIR/fwup/
    rm -f *.nupkg
    export ChocolateyInstall=$DEPS_INSTALL_DIR/chocolatey
    $ChocolateyInstall/console/choco.exe pack --allow-unofficial fwup.nuspec
    cd $BASE_DIR
    rm -f *.nupkg
    cp $FWUP_INSTALL_DIR/fwup/*.nupkg .
elif [ "$CROSS_COMPILE" = "arm-linux-gnueabihf" ]; then
    # Build Raspberry Pi package

    # Fix directory permissions for packaging
    find $FWUP_STAGING_DIR -type d | xargs chmod 755

    # Debian requires compressed man pages
    # NOTE: Even though building man pages is optional, it is
    #       an error if man pages don't exist when creating packages
    gzip -9 -f $FWUP_STAGING_DIR/usr/share/man/man1/fwup.1

    # Build Raspbian package
    rm -f fwup_*.deb
    fpm -s dir -t deb -v $FWUP_VERSION -n fwup -m "$FWUP_MAINTAINER" \
        --license "$FWUP_LICENSE" --description "$FWUP_DESCRIPTION" \
        --depends "libc6 (>= 2.4)" --deb-priority optional --category devel \
        -a armhf \
        --url "$FWUP_HOMEPAGE" --vendor "$FWUP_VENDOR" -C $FWUP_STAGING_DIR

fi

