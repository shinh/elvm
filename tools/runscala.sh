#!/bin/sh

set -e

mkdir -p $1_dir
cp $1 $1_dir/Main.scala
scalac $1_dir/Main.scala
cd $1_dir && scala Main
