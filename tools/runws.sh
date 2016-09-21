#!/bin/sh

set -e

Whitespace/whitespace.out $1 -t > $1.c
tinycc/tcc -Btinycc $1.c -o $1.c.exe
./$1.c.exe
