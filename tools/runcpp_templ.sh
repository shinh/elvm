#!/bin/bash

set -e

dir=$(mktemp -d)
mkdir -p $dir
cp $1 $dir/test.cpp
cp ./target/cpp_templ_lib.hpp $dir
infile=${dir}/input.txt

cat > $infile

if [ ! -s $infile ]; then
    echo '""' > $infile
else
  sed -i '1s/^/R"(/' $infile
  echo ')"' >> $infile
fi

g++-6 ${dir}/test.cpp -o $1.exe -ftemplate-depth=2147483647
rm -fr $dir
./$1.exe
