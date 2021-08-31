# Building from source on Linux

## TLDR

This works on Ubuntu 20.04:

```sh
sudo apt-get install build-essential autoconf pkg-config libtool mtools unzip zip help2man libconfuse-dev libarchive-dev xdelta3 dosfstools
./configure
make -j4
make check
sudo make install
```

Users on earlier versions of Ubuntu will get a warning about libconfuse being
old. That's usually not a problem if you are only creating .fw files. Having a
current version of libconfuse is important on the device side.

## More information

While `fwup` is not a particularly complicated program, it is not trivial to
build due to a couple project dependencies. If you are not comfortable with
building applications from source, please consider one of the pre-build
packages.

Normally Linux applications use dynamic linking. The recommendation for `fwup`
is to statically link to avoid pulling in out-of-date versions of `libconfuse`.
The next section covers static linking. If you are a package maintainer or are
not worried about old versions, read the second section. At the moment, using an
old version of `libconfuse` results in one broken regression test that normally
is not a problem for host use.

## Common dependencies

Before building, make sure that your system has some basic packages installed.
At least the following are required for building and running the regression
tests:

```sh
sudo apt-get install build-essential autoconf pkg-config libtool mtools unzip zip help2man xdelta3 dosfstools
```

## Static build

A script is included to download and statically build the dependencies. The rest
is a fairly typical build process. Modify the `./configure` line if necessary to
change the installation location.

```sh
./scripts/download_deps.sh
./scripts/build_deps.sh
./autogen.sh    # only necessary if building from a git source tree
PKG_CONFIG_PATH=$PWD/build/host/deps/usr/lib/pkgconfig ./configure --enable-shared=no
make
make check
make install
```

## Shared build

### Installing dependencies

The installation instructions vary based on your Linux distribution. Please
verify each step in case things have changed since this was written (and create
a pull request on GitHub if you could.)

On Ubuntu:

```sh
# The version of libconfuse available in apt is too old
curl -L https://github.com/martinh/libconfuse/releases/download/v3.2.2/confuse-3.2.2.tar.gz | tar -xz -C /tmp
pushd /tmp/confuse-3.2.2
./configure && make && sudo make install
popd
rm -rf /tmp/confuse-3.2.2

sudo apt-get install libarchive-dev
```

On CentOS 6:

```sh
# The version of libconfuse available in yum is too old
curl -L https://github.com/martinh/libconfuse/releases/download/v3.2.2/confuse-3.2.2.tar.gz | tar -xz -C /tmp
pushd /tmp/confuse-3.2.2
./configure && make && sudo make install
popd
rm -rf /tmp/confuse-3.2.2

# The version of libarchive available in yum is too old
curl -L http://www.libarchive.org/downloads/libarchive-3.1.2.tar.gz | tar -xz -C /tmp
pushd /tmp/libarchive-3.1.2
./configure && make && sudo make install
popd
rm -rf /tmp/libarchive-3.1.2

# Assuming all of the libraries were installed to /usr/local/lib
sudo ldconfig /usr/local/lib

# Building fwup from source requires autotools
sudo yum install autoconf automake libtool
```

On CentOS 7:

```sh
# The version of libconfuse available in yum is too old
curl -L https://github.com/martinh/libconfuse/releases/download/v3.2.2/confuse-3.2.2.tar.gz | tar -xz -C /tmp
pushd /tmp/confuse-3.2.2
./configure && make && sudo make install
popd
rm -rf /tmp/confuse-3.2.2

sudo yum install libarchive-devel
```

On Alpine:

```sh
apk add confuse-dev libarchive-dev
```

### Building

On Linux:

```sh
cd fwup
./autogen.sh
./configure
make
make check
sudo make install
```
