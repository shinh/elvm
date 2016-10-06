#!/bin/sh

set -e

mkdir -p $1_dir
cp $1 $1_dir/Main.java
javac $1_dir/Main.java
cd $1_dir && java Main
