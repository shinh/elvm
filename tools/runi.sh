#!/bin/sh

ick -b -o $1 > $1.c
tinycc/tcc -Btinycc -I/usr/include/ick-0.29 $1.c -lick -o $1.c.exe
$1.c.exe
