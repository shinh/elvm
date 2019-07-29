#!/bin/sh

set -e

lmfao/lmfao $1 -o $1.mu > /dev/null
Unshackled $1.mu

# faster interpreter (highly recommended): https://lutter.cc/unshackled/interpreter.html
