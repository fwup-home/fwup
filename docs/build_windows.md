# Building from Source for Windows

Building on Windows is complicated due to the required 3rd party libraries.
As a result, the Windows build is done on Linux using a crosscompile and
Wine to run the regression tests.

First, make sure that 32-bit builds work and install dependencies:

    sudo dpkg --add-architecture i386
    sudo apt-get update
    sudo apt-get install build-essential autoconf pkg-config libtool mtools unzip wine gcc-mingw-w64-x86-64

Then, run a static build using the mingw compiler:

    export CROSS_COMPILE=x86_64-w64-mingw32
    export CC=$CROSS_COMPILE-gcc

    # These are needed for building choco (the Windows package manager).
    # You can skip this next part if you're not interested in the fwup
    # Chocolatey package. Export SKIP_PACKAGE=true to avoid the error
    # when running ./scripts/build_pkg.sh below.

    sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF
    echo "deb http://download.mono-project.com/repo/debian wheezy/snapshots/3.12.0 main" | sudo tee /etc/apt/sources.list.d/mono-xamarin.list
    sudo apt-get update
    sudo apt-get install -qq mono-devel mono-gmcs nuget

    ./autogen.sh    # only necessary if building from a git source tree
    ./scripts/build_pkg.sh

The script should exit successfully and you should have an `fwup.exe` binary, and an `fwup.*.nupkg` Chocolatey package.

To install a pre-release Chocolatey Package:

    choco install fwup.*.nupkg -pre
