#!/bin/bash

set -e

dir=$(mktemp -d)
mkdir -p $dir
cp $1 $dir
infile=${dir}/input.txt

cat > $infile

if [ ! -s $infile ]; then
    echo '""' > $infile
else
  sed -i '1s/^/R"(/' $infile
  echo ')"' >> $infile
fi

g++ -fconstexpr-loop-limit=1000000 ${dir}/$(basename $1) -o $1.exe
rm -fr $dir
./$1.exe
