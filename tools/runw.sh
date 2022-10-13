#!/bin/bash

set -e


if [ ! -e out/grass_ml ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir

    git clone https://gist.github.com/woodrush/3d85a6569ef3c85b63bfaf9211881af6
    mv 3d85a6569ef3c85b63bfaf9211881af6/grass.ml .
    ocamlc -o grass_ml grass.ml
    mv grass_ml ${orig_dir}/out

    cd $orig_dir
    rm -rf $dir
fi

out/grass_ml $1
