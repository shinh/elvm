#!/bin/sh

set -e

if [ ! -e out/unlambda ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir

    wget https://cdn.jsdelivr.net/gh/irori/unlambda@844fa552/unlambda.c
    gcc -O2 unlambda.c -o unlambda

    mv unlambda ${orig_dir}/out
    cd $orig_dir
    rm -fr $dir
fi

out/unlambda $1
