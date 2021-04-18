#!/bin/sh

set -e

orig_dir=$(pwd)
temp_dir=$(mktemp -d)

python3=$(if [ -e "$(which python3)" ]; then echo python3; else echo python; fi)

# Generate the final code
"${python3}" ./tools/qftasm/qftasm_pp.py $1 > $temp_dir/tmp.qftasm

# Run the code
"${python3}" ./tools/qftasm/qftasm_interpreter.py $temp_dir/tmp.qftasm

rm -rf ${temp_dir}
