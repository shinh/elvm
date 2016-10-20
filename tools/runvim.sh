#!/bin/sh

# Runs Vim script code by Ex mode with silent mode.
# Only readfile() can treat binary input (e.g. input without EOL at the end of file) properly in Vim.
# This script saves stdin to /tmp/*.out and outputs the result (stdout) to /tmp/*.out after :source the script.

out=/tmp/$(basename $1).out

cat > ${out}
TERM=dumb vim -X -N -b -n -u NONE -i NONE -V1 -e -s --cmd "call setline(1, readfile('${out}', 'b')) | set noeol | source $1 | w! ${out} | qa!" > /dev/null 2>&1
cat ${out}
