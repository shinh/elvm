#!/bin/sh

set -e


if [ ! -e out/packbits ]; then
    gcc tools/packbits.c -o packbits > /dev/null
    mv packbits out
fi

if [ ! -e out/blc ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir

    git clone https://github.com/tromp/AIT
    cd AIT
    cabal install dlist --lib > /dev/null
    cabal install mtl-2.2.2 --lib > /dev/null
    make blc > /dev/null

    cd $orig_dir
    mv $dir/AIT/blc out
    rm -rf $dir
fi

if [ ! -e out/uni ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir
    git clone https://github.com/melvinzhang/binary-lambda-calculus
    cd binary-lambda-calculus
    make > /dev/null

    cd $orig_dir
    mv $dir/binary-lambda-calculus/uni out/uni
    rm -rf $dir
fi

# Required for parsing large programs
ulimit -s 524288

(out/blc blc $1 | out/packbits; cat) | out/uni -o
