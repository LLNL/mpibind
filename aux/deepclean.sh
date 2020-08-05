#!/bin/bash

#ACLOCAL_PATH=$ACLOCAL_PATH:$(spack location -i pkgconfig)/share/aclocal autoreconf --install

# Todo: I should manage this through 'clean-local:' in Makefile.am
# https://www.gnu.org/software/automake/manual/html_node/Extending.html#Extending 

set -x

# make maintainer-clean
make distclean

rm Makefile.in
rm src/Makefile.in
rm aclocal.m4
rm -r autom4te.cache
rm -r config
rm configure

rm -r install

