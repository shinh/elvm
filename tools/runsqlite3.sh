#!/bin/bash

set -e

dir=$(mktemp -d)
mkdir -p $dir
infile=${dir}/input.txt
cat > $infile

cp $1 ${dir}/
cd $dir
cat $(basename $1) | sqlite3 > /dev/null
cat output.txt
rm -fr $dir > /dev/null
