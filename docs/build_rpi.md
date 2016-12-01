# Building from Source for the Raspberry Pi

The Raspberry Pi version can be built both on Raspbian and cross-compiled on an
x86 Linux machine. However, packaging the image in a `.deb` file appears to only work on
x86 since `fpm` doesn't install for me.

On the Raspbian, the easiest way to build is to run:

   sudo apt-get install autoconf libtool mtools unzip zip help2man
    ./autogen.sh    # only necessary if building from a git source tree
    ./scripts/build_pkg.sh # This will fail when packaging (ignore this)
    make install

The `build_pkg.sh` script statically links `fwup` to reduce some complexity with
obtaining uptodate versions of the dependencies. See the [Linux build
instructions](build_linux.md) for more Linux information.

To build a `.deb` package for Raspbian on an x86 machine, do the following:

    sudo apt-get update
    sudo apt-get install build-essential autoconf pkg-config libtool mtools unzip
    git clone git://github.com/raspberrypi/tools.git

Then, run a static build using the RPi cross-compiler:

    export PATH=<path to tools>/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin
    export CROSS_COMPILE=arm-linux-gnueabihf
    export CC=$CROSS_COMPILE-gcc
    ./autogen.sh    # only necessary if building from a git source tree
    ./scripts/build_pkg.sh

The script should exit successfully and you should have an `.deb` package. To
recompile from now on, it is sufficient to just run `make`.

To run the unit tests against the Raspbian version of `fwup` on an x86 computer,
install qemu:

    sudo apt-get install qemu binfmt-support qemu-user-static

This should set up the magic binfmt_misc feature on Linux so that if you invoke
the armhf version of `fwup`, it will call into qemu. Double check that the
binfmt feature is configured:

    update-binfmts --display

The next step is to point qemu to use the Raspbian version of the C library:

    export QEMU_LD_PREFIX=<path to tools>/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/libc/lib/arm-linux-gnueabihf/

Now you should be able to run `make check`
