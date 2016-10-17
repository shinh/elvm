#!/bin/sh

cat > /tmp/out
TERM=dumb vim -X -N -b -n -u NONE -i NONE -V1 -e -s --cmd "call setline(1, readfile('/tmp/out', 'b')) | set noeol | source $1 | w! /tmp/out | qa!" > /dev/null 2>&1
cat /tmp/out
