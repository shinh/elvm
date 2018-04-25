#!/bin/sh

set -e

# mkdir -p $1_dir
# cp $1 $1_dir/Main.scala
runghc $1 -o a.out
# ./a.out
# scala Main