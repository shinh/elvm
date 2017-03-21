#!/bin/bash

set -e

dir=$(mktemp -d)
mkdir -p $dir
cp $1 $dir/$(basename $1).cpp
infile=${dir}/input.txt

cat > $infile

if [ ! -s $infile ]; then
    echo '""' > $infile
else
  sed -i '1s/^/R"(/' $infile
  echo ')"' >> $infile
fi

g++ -std=c++11 ${dir}/$(basename $1).cpp -o $1.exe -ftemplate-depth=2147483647
rm -fr $dir
./$1.exe
