## Building and installing from source

If you are using Homebrew as your packaging manager, building `fwup`
should be straightforward. Just run the following from a shell prompt:

    brew install confuse libarchive libsodium

    cd fwup
    ./autogen.sh
    # This assumes that libarchive, libconfuse and libsodium were installed via
    # homebrew.
    PKG_CONFIG_PATH="/usr/local/opt/libarchive/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure
    make
    sudo make install

