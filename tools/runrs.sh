#!/bin/sh

set -e

name="`basename -s .rs $1 | sed -E "s/\./_/g"`.rs"
path=`dirname $1`/$name
cp $1 $path
rustc $path -o $path.exe
$path.exe
