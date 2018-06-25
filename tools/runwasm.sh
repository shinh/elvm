#!/bin/sh
set -e
# The wasm ELVM backend generates WebAssembly Text Format (.wat) files.

# First we translate them to WebAssembly binary format (.wasm)
wat2wasm $1 -o $1.wasm
# And then run them
nodejs tools/run_compiled_wasm.js $1.wasm
