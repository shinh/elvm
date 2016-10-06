#!/bin/sh

set -e

#convert $1 $1.png
(cat /dev/stdin && /bin/echo -ne "\0") | npiet/npiet -q $1
exit

# http://www.matthias-ernst.eu/pietcompiler.html
PietCompiler/PietCompiler $1 -o $1.exe > /dev/null
$1.exe
