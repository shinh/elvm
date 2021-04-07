#!/bin/sh

orig_dir=$(pwd)
temp_dir=$(mktemp -d)

# Generate the final code
python ./tools/qftasm/qftasm_pp.py $1 > $temp_dir/tmp.qftasm

# Run the code
python ./tools/qftasm/qftasm_interpreter.py $temp_dir/tmp.qftasm

rm -rf ${temp_dir}
