#!/bin/bash

#spack load pkgconf
spack load m4/hwz6doo
spack load autoconf/kcfww4q
spack load automake/j4uvdos
spack load libtool/fac54ff

spack load py-cffi

export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$HOME/work/repos/libtap/install/lib/pkgconfig

# pycotap is install under ~/.local/
#export PYTHONPATH=$PYTHONPATH:$HOME/work/repos/pycotap/install/lib/python3.8/site-packages
