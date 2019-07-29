#!/bin/sh

set -e

lmfao/lmfao $1 -o $1.mu > /dev/null
Unshackled $1.mu

# faster interpreter: https://lutter.cc/unshackled/Unshackled.c
