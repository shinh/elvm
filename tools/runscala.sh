#!/bin/sh

set -e

mkdir -p $1_dir
cp $1 $1_dir/Main.scala
scalac -J-Xms256m -J-Xmx512m $1_dir/Main.scala
scala Main