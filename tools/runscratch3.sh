#!/bin/sh

# Scratch backend emits "project.json" that contains AST of the script.
# The project file of Scratch3.0 consists of the JSON file and some images
# (in this case empty backdrop svg).
# They are zip-archived and renamed to "*.sb3".
# This tool makes the ".sb3" file and execute it with `run_scratch.js` (nodejs)

orig_dir=$(pwd)
temp_dir=$(mktemp -d)

# Generate sb3
cp "$1" "${temp_dir}/test.json"
./tools/gen_scratch_sb3.sh "${temp_dir}/test.json" -name "test"

# Execute it
nodejs "tools/run_scratch.js" "${temp_dir}/test.sb3"

rm -rf ${temp_dir}
