#!/bin/sh

set -e


if [ ! -e out/lazyk ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir

    git clone https://github.com/irori/lazyk
    cd lazyk
    make > /dev/null

    mv lazyk ${orig_dir}/out
    cd $orig_dir
    rm -fr $dir
fi

out/lazyk -u $1
