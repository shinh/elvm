#!/bin/sh

set -e

if [ ! -e out/unlambda ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir

    wget http://users.math.cas.cz/~jerabek/unlambda/unl.c
    gcc -O -g unl.c -o unlambda

    mv unlambda ${orig_dir}/out
    cd $orig_dir
    rm -fr $dir
fi

out/unlambda $1
