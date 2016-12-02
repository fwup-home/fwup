# Building from source on FreeBSD

Sadly, I don't have an autobuilder for any of the BSDs, so these instructions
are not regularly verified. Please let me know if FreeBSD, NetBSD, or OpenBSD
breaks and especially if there's an autobuilder like Travis-CI for those OS's.
Luckily, building fwup is not that complicated.

## System prep

Before building, some packages are required for building and running regression
tests.

On FreeBSD, run:

    pkg install autoconf pkgconf zip help2man mtools base64

On OpenBSD, run:

    pkg_add autoconf automake zip unzip help2man mtools base64

On NetBSD, run:

    pkg_add autoconf automake libtool pkg-config zip help2man mtools base64

## Building

On other platforms, `fwup` can be built statically or dynamically. At
the moment, only the dynamic (shared library) version is supported on BSD.
To build the dynamic version, install the shared library dependencies:

    # Install compile-time and run-time dependencies

    ## FreeBSD
    pkg install libarchive libsodium

    ## OpenBSD
    pkg_add libarchive libsodium

    ## NetBSD
    pkg_add confuse libarchive libsodium

    # The FreeBSD and OpenBSD versions of libconfuse are very old (v2.7)
    # 2.8 or later is needed and 3.0 is much preferred.
    curl -O -L https://github.com/martinh/libconfuse/releases/download/v3.0/confuse-3.0.tar.gz
    tar xzf confuse-3.0.tar.gz
    cd confuse-3.0
    ./configure && make && sudo make install

If you are building `fwup` from the git repository, you'll need to
generate the `./configure` script. Do the following:

    cd fwup
    ./autogen.sh

Now you can build and install `fwup` using the normal Unix build recipe:

    cd fwup
    ./configure
    # if ./configure fails to find libconfuse, try running:
    # export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
    make
    make check
    sudo make install

