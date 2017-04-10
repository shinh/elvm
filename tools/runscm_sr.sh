#!/bin/bash

set -e

dir=$(mktemp -d)
mkdir -p $dir
cp $1 $dir/$(basename $1).scm
infile=${dir}/input.scm

cat | ruby tools/makeinput_scm_sr.rb > $infile

cd ${dir}; gosh $(basename $1).scm
rm -fr $dir
