#!/bin/sh

set -e

# http://www.matthias-ernst.eu/pietcompiler.html
PietCompiler/PietCompiler $1 -o $1.exe > /dev/null
$1.exe
