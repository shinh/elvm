#!/bin/sh

set -e

crystal build $1 -o $1.exe
./$1.exe
