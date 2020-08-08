#!/bin/bash

spack load pkgconf
spack load m4
spack load autoconf
spack load automake
spack load libtool

export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$HOME/work/repos/libtap/install/lib/pkgconfig
