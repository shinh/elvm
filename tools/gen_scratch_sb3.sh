#!/bin/bash

# This tool generates Scratch 3.0 project file (with ".sb3" extension)
# from project json that Scratch3 backend output.
#
# Usage: ./gen_scratch_sb3.sh [the_output_json] (-name [project-name])
#
# Example:
#     $ ./out/elc -scratch3 test/01putc.eir > 01putc.eir.scratch3
#     $ ./tools/gen_scratch_sb3.sh 01putc.eir.scratch3
#     $ ls 01putc.eir.scratch3.sb3
#     01putc.eir.scratch3.sb3

USAGE="Usage: $0 [project-json] (-name [project-name])"

if [ "$#" -ne 1 ] && [ "$#" -ne 3 ]; then
  echo "$USAGE"
  exit 1
fi

if [ ! -e "$1" ]; then
  echo "'$1' not found."
  echo "$USAGE"
  exit 1
fi

output_path="$1.sb3"
if [ "$#" -eq 3 ] && [ "$2" = "-name" ]; then
  output_path="$(dirname $1)/$3.sb3"
fi

# Background Image
BACKDROP="fcb8546c50e11d422b78fd55e34d7e7e.svg"
BACKDROP_SVG='<svg version="1.1" width="2" height="2" viewBox="-1 -1 2 2" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"></svg>'

# Create sb3
temp_dir=$(mktemp -d)
echo -n "${BACKDROP_SVG}" > "${temp_dir}/${BACKDROP}"
cp "$1" "${temp_dir}/project.json"
zip -j "${output_path}" "${temp_dir}/${BACKDROP}" "${temp_dir}/project.json" > /dev/null 2>&1
rm -rf ${temp_dir}
