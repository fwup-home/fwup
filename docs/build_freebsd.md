# Building from source on FreeBSD

Sadly, we don't have an autobuilder for FreeBSD, so these instructions
are not regularly verified. Please let me know if FreeBSD (or other 
BSD) builds break and especially if there's an autobuilder like
Travis-CI for FreeBSD out there). Luckily, building fwup is not that
complicated.

## System prep

Before building, make sure that your system has some basic packages installed.
At least the following are required for building and running the regression
tests:

    pkg install autoconf pkgconf zip help2man mtools base64

## Building

On other platforms, `fwup` can be built statically or dynamically. At
the moment, only the dynamic (shared library) version is supported on FreeBSD.
To build the dynamic version, install needed shared libraries:

    # Install compile-time and run-time dependencies
    pkg install libarchive libsodium

    # The FreeBSD version of libconfuse is very old (v2.7)
    # 2.8 or later is needed and 3.0 has some really good
    # improvements, so use it.
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

