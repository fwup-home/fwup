# Building from Source for Windows

Building on Windows is complicated due to the required 3rd party libraries.
As a result, the Windows build is done on Linux using a cross-compile and
Wine to run the regression tests.  
Alternatively, this procedure is also done on [Ubuntu 20.04 LTS on Windows](https://www.microsoft.com/en-us/p/ubuntu-2004-lts/9n6svws3rx71?activetab=pivot:overviewtab)

## install dependencies

First, make sure that 32-bit builds work and install dependencies:

```sh
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install build-essential autoconf pkg-config libtool mtools unzip zip xdelta3 wine wine-binfmt gcc-mingw-w64-x86-64

# I had to manually import the wine binfmt description on one install. You'll
# know this is an issue if running `fwup.exe` doesn't work.
sudo update-binfmts --import /usr/share/binfmts/wine
```

## build executable

Then, run a static build using the mingw compiler:

```sh
export CROSS_COMPILE=x86_64-w64-mingw32
export CC=$CROSS_COMPILE-gcc

./autogen.sh    # only necessary if building from a git source tree
./scripts/build_static.sh
```

When all tests passed (or skipped), you successfully get a `fwup.exe` on the current directory.

## build nupkg for Chocolatey

If you'd like to build an `fwup.*.nupkg` for Chocolatey, try running the
build_pkg.sh script. Chocolatey is tricky to install, so see
scripts/ubuntu_install_chocolatey.sh if something goes wrong.

```sh
./scripts/build_pkg.sh
```

The script should exit successfully and you should have a `fwup.*.nupkg` Chocolatey package.  
You may set `$DISPLAY` to avoid an error about `Wine Mono is not installed`.

To install a pre-release Chocolatey Package:

```sh
choco install fwup.*.nupkg -pre
```
