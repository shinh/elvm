#!/bin/sh

set -e


FILE=$(basename $1)

# XXX do this in a makefile so it only happens once
cp -r tools/wmc/src/crt out/wmc

out/wmc/wmc -s $1 -o out/wmc/crt
make -C out/wmc/crt > /dev/null
mv out/wmc/crt/wm out/wmc/${FILE}.c.exe

./out/wmc/${FILE}.c.exe
