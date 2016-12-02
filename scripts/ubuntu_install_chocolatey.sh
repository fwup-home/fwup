#!/bin/bash

#
# Install the Windows packaging tools on Ubuntu
#

set -e

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

source $BASE_DIR/scripts/common.sh

# Install mono if necessary
if [ ! -f /usr/bin/nuget ]; then
   sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF
   echo "deb http://download.mono-project.com/repo/debian wheezy/snapshots/3.12.0 main" | sudo tee /etc/apt/sources.list.d/mono-xamarin.list
   sudo apt-get update
   sudo apt-get install -qq mono-devel mono-gmcs nuget
fi

# Ensure that some directories exist
mkdir -p $DOWNLOAD_DIR
mkdir -p $DEPS_DIR

# Download Chocolatey
if [ ! -e $DOWNLOAD_DIR/choco-$CHOCO_VERSION.tar.gz ]; then
    curl -L -o $DOWNLOAD_DIR/choco-$CHOCO_VERSION.tar.gz https://github.com/chocolatey/choco/archive/$CHOCO_VERSION.tar.gz
fi

# Build Chocolatey if not already built
if [ ! -e $DEPS_INSTALL_DIR/chocolatey/console/choco.exe ]; then
    cd $DEPS_DIR
    rm -fr choco-*
    tar xf $DOWNLOAD_DIR/choco-$CHOCO_VERSION.tar.gz
    cd choco-$CHOCO_VERSION
    nuget restore src/chocolatey.sln
    chmod +x build.sh
    ./build.sh -v
    cp -Rf code_drop/chocolatey $DEPS_INSTALL_DIR/
fi
