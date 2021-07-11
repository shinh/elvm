#!/bin/sh

set -e

gfortran $1 -o $1.exe
./$1.exe
