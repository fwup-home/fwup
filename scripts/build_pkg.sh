#!/usr/bin/env bash

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

create_fwup_deb() {
    local FWUP_ARCH=$1

    local FWUP_DEB_NAME="fwup_${FWUP_VERSION}_${FWUP_ARCH}"
    local FWUP_DEB_DIR="$BUILD_DIR/$FWUP_DEB_NAME"

    # Clear and recreate the DEB directory structure
    rm -fr "$FWUP_DEB_DIR"
    mkdir -p "$FWUP_DEB_DIR/DEBIAN"
    cp -r "$FWUP_STAGING_DIR"/* "$FWUP_DEB_DIR"

    # Fix directory permissions for packaging
    find "$FWUP_DEB_DIR" -type d | xargs chmod 755

    # Debian requires compressed man pages
    # Check for the existence of the man page before attempting to gzip it
    if [ ! -f "$FWUP_DEB_DIR/usr/share/man/man1/fwup.1.gz" ]; then
        if [ -f "$FWUP_DEB_DIR/usr/share/man/man1/fwup.1" ]; then
            gzip -9 -f "$FWUP_DEB_DIR/usr/share/man/man1/fwup.1"
        else
            echo "Error: Man page does not exist and is required."
            return 1
        fi
    fi

    # Create control file
    cat > "$FWUP_DEB_DIR/DEBIAN/control" << EOF
Package: fwup
Version: $FWUP_VERSION
License: $FWUP_LICENSE
Vendor: $FWUP_VENDOR
Architecture: $FWUP_ARCH
Maintainer: $FWUP_MAINTAINER
Depends: libc6 (>= 2.4)
Section: devel
Priority: optional
Homepage: $FWUP_HOMEPAGE
Description: $FWUP_DESCRIPTION
EOF

    # Create and gzip the changelog
    mkdir -p "$FWUP_DEB_DIR/usr/share/doc/fwup"
    gzip > "$FWUP_DEB_DIR/usr/share/doc/fwup/changelog.gz" << EOF
fwup ($FWUP_VERSION) ; urgency=medium

  * Automatically created package.

 -- $FWUP_MAINTAINER  $(date -R)
EOF

    # Generate md5sums for the files
    (cd "$FWUP_DEB_DIR" && find usr -type f | sort | xargs md5sum > "$FWUP_DEB_DIR/DEBIAN/md5sums")

    # Build the package
    dpkg-deb -Zgzip --build "$FWUP_DEB_DIR"
    mv "$BUILD_DIR/$FWUP_DEB_NAME.deb" .
}

# Package fwup
FWUP_VERSION=$(cat $BASE_DIR/VERSION)
if [ -z "$CROSS_COMPILE" ]; then
    # Build Linux packages
    rm -f fwup_*.deb fwup-*.rpm

    # Fix directory permissions for packaging
    find $FWUP_STAGING_DIR -type d | xargs chmod 755

    create_fwup_deb amd64
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
    # such a pain and kills CircleCI builds randomly and frequently.
    $BASE_DIR/scripts/ubuntu_install_chocolatey.sh

    cd $FWUP_INSTALL_DIR/fwup/
    rm -f *.nupkg
    export ChocolateyInstall=$DEPS_INSTALL_DIR/chocolatey
    chmod +x $ChocolateyInstall/console/choco.exe
    mono $ChocolateyInstall/console/choco.exe pack --allow-unofficial fwup.nuspec
    cd $BASE_DIR
    rm -f *.nupkg
    cp $FWUP_INSTALL_DIR/fwup/*.nupkg .
elif [ "$CROSS_COMPILE" = "arm-linux-gnueabihf" ]; then
    # Build Raspberry Pi package


    # Build Raspbian package
    rm -f fwup_*.deb
    create_fwup_deb armhf
fi

