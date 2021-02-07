# OSX

## Building and installing with Homebrew

If you are using Homebrew as your packaging manager, you're probably better off
running `brew install fwup` since the Homebrew maintainers do a remarkable job
of keeping their version of `fwup` up-to-date. If you want to build `fwup` from
source, though, here's the recipe:

```bash
# The following packages are needed to build
$ brew install confuse libarchive pkg-config automake

# The following packages are needed for the regression tests
$ brew install coreutils mtools xdelta

$ cd fwup
$ ./autogen.sh
$ PKG_CONFIG_PATH="$(brew --prefix libarchive)/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure
$ make
$ sudo make install

# Run the tests
$ make check
```

## Static OSX builds

If you need to run `fwup` on systems without Homebrew, it's possible to create a
version that only requires standard OSX-supplied shared libraries. Here's how:

```bash
# The following packages are needed to build
$ brew install confuse libarchive pkg-config automake

# The following packages are needed for the regression tests
$ brew install coreutils mtools xdelta

$ cd fwup
$ ./scripts/download_deps.sh
$ ./scripts/build_deps.sh
$ ./autogen.sh
$ PKG_CONFIG_PATH=$PWD/build/host/deps/usr/lib/pkgconfig ./configure --enable-shared=n
$ make
$ make check

# Verify shared library usage (optional)
$ objdump -macho -dylibs-used src/fwup

# Install
$ make install
```
