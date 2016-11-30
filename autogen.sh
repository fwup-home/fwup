#!/bin/sh

set -e

autoreconf --install

if [ -d "/mnt/c/Users" ]; then
    # Copy pkg.m4 for Windows 10 bash mode use. If this isn't done,
    # you'll PKGCONFIG lines won't be substituted in ./configure
    cp /usr/share/aclocal/pkg.m4 m4
fi

