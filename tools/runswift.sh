#!/bin/sh

set -e

swiftc $1 -o $1.exe
./$1.exe
