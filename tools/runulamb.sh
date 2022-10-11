#!/bin/bash

set -e


if [ ! -e out/packbits ]; then
    gcc -O2 tools/packbits.c -o packbits
    mv packbits out
fi

if [ ! -e out/clamb ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir

    git clone https://github.com/irori/clamb
    cd clamb
    make clamb > /dev/null
    mv clamb ${orig_dir}/out

    cd $orig_dir
    rm -rf $dir
fi


out/clamb -u <(cat $1 | out/packbits)
