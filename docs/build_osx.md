## Building and installing from source

If you are using Homebrew as your packaging manager, building `fwup`
should be straightforward. Just run the following from a shell prompt:

```shell
# The following packages are needed to build
$ brew install confuse libarchive libsodium pkg-config automake

# The following packages are needed for the regression tests
$ brew install coreutils mtools

$ cd fwup
$ ./autogen.sh
$ PKG_CONFIG_PATH="/usr/local/opt/libarchive/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure
$ make
$ sudo make install

# Run the tests
$ make check
```
