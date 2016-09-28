#!/bin/sh

set -e

./piet-assembler $1 > $1.ppm
convert $1.ppm $1.png
./npiet $1.png
