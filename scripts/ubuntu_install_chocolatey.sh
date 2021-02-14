#!/bin/bash

#
# Install the Windows packaging tools on Ubuntu
#

set -e

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
USE_PREBUILT=false

source $BASE_DIR/scripts/common.sh

# Install mono if necessary
if [ ! -f /usr/bin/nuget ]; then
    apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF
    echo "deb https://download.mono-project.com/repo/ubuntu stable-focal main" | tee /etc/apt/sources.list.d/mono-official-stable.list
    apt update
    apt install -qq mono-devel nuget --yes
fi

# Ensure that some directories exist
mkdir -p $DOWNLOAD_DIR
mkdir -p $DEPS_DIR

# Download Chocolatey
if $USE_PREBUILT; then
    if [ ! -e $DOWNLOAD_DIR/choco-${CHOCO_VERSION}-binaries.tar.gz ]; then
        # Download a prebuilt version
        cd $DOWNLOAD_DIR
        curl -LO http://files.troodon-software.com/choco/choco-${CHOCO_VERSION}-binaries.tar.gz
        cd $BASE_DIR
    fi
else
    if [ ! -e $DOWNLOAD_DIR/choco-$CHOCO_VERSION.tar.gz ]; then
        # Download the source
        curl -L -o $DOWNLOAD_DIR/choco-$CHOCO_VERSION.tar.gz https://github.com/chocolatey/choco/archive/$CHOCO_VERSION.tar.gz
    fi
fi

# Build Chocolatey if not already built
if [ ! -e $DEPS_INSTALL_DIR/chocolatey/console/choco.exe ]; then

    if $USE_PREBUILT; then
        tar xf $DOWNLOAD_DIR/choco-${CHOCO_VERSION}-binaries.tar.gz -C $DEPS_INSTALL_DIR
    else
        cd $DEPS_DIR
        rm -fr choco-*
        tar xf $DOWNLOAD_DIR/choco-$CHOCO_VERSION.tar.gz
        cd choco-$CHOCO_VERSION
        patch -p1 < "$BASE_DIR/scripts/patches/choco/0001-Comment-out-failing-test-on-Travis.patch"
        nuget restore src/chocolatey.sln
        chmod +x build.sh
        ./build.sh -v
        cp -Rf code_drop/chocolatey $DEPS_INSTALL_DIR/
    fi
fi
