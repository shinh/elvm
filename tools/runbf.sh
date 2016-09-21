#!/bin/sh

set -e

out/bfopt -c $1 $1.c
tinycc/tcc -Btinycc $1.c -o $1.c.exe
./$1.c.exe
