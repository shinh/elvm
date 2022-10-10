#!/bin/bash

set -e


if [ ! -e out/packbits ]; then
    gcc -O2 tools/packbits.c -o packbits
    mv packbits out
fi

if [ ! -e out/uni ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir

    git clone https://github.com/melvinzhang/binary-lambda-calculus
    cd binary-lambda-calculus
    make > /dev/null
    mv uni ${orig_dir}/out

    cd $orig_dir
    rm -rf $dir
fi

# Required for parsing large programs
ulimit -s 524288

(cat $1 | ./out/packbits; cat - ) | out/uni -o
