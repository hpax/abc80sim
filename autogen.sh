#!/bin/sh -xe
#
# Simple script to run the appropriate autotools from a repository.
#
autolib=`automake --print-libdir`
for prg in install-sh compile config.guess config.sub; do
    cp -f "$autolib"/"$prg" autoconf
done
autoreconf -i -B autoconf
rm -rf autom4te.cache config.log config.status config/config.h Makefile

