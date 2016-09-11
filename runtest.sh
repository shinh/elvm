#!/bin/bash

set -e

out=$1
tmp=${out}.tmp
name=$(basename ${out} | sed 's/\..*//')
shift
cmd="$@"

ins=$(/bin/ls */${name}*.in 2> /dev/null || true)

if [ -z "${ins}" ]; then
    "${cmd}" > ${tmp}
else
    rm -f ${tmp}
    for i in ${ins}; do
        echo "=== ${i} ===" >> ${tmp}
        "${cmd}" < ${i} >> ${tmp}
        echo >> ${tmp}
    done
fi

mv ${tmp} ${out}
