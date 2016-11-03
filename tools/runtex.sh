#!/bin/bash

# Runs TeX code.

cp $1 /tmp/${1##*/}
cd /tmp
cat /dev/stdin | od -A n -t d1 -w1 -v | tr -d " " > /tmp/${1##*/}.in
echo 0 >> /tmp/${1##*/}.in
cat /tmp/${1##*/}.in  | tex ${1##*/} > /dev/null 2>&1
if [ -s ${1##*/}.elvm.out ]; then
    cat ${1##*/}.elvm.out | xargs -n1 printf "%03o\n" | xargs -I_ printf \\_
fi
