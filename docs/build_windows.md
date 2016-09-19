# Building from Source for Windows

Building on Windows is complicated due to the required 3rd party libraries.
As a result, the Windows build is done on Linux using Wine.

First, make sure that 32-bit builds work and install dependencies:

    sudo dpkg --add-architecture i386
    sudo apt-get update
    sudo apt-get install build-essential autoconf pkg-config libtool mtools unzip wine gcc-mingw-w64-x86-64

Then, run a static build using the mingw compiler:

    export CROSS_COMPILE=x86_64-w64-mingw32
    export CC=$CROSS_COMPILE-gcc
    ./autogen.sh    # only necessary if building from a git source tree
    ./scripts/build_pkg.sh

The script should exit successfully and you should have an `fwup.exe` binary, and an `fwup.*.nupkg` Chocolatey package.

To install a pre-release Chocolatey Package:

    choco install fwup.*.nupkg -pre
