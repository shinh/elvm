#!/bin/sh

set -e

tinycc/tcc -Btinycc $1 -o $1.exe
./$1.exe
