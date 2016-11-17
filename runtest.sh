#!/bin/bash

set -e

out=$1
tmp=${out}.tmp
name=$(basename ${out} | sed 's/\..*//')
shift
cmd="$@"
ext=$(echo ${cmd} | sed 's/.*\.//')

ins=$(/bin/ls */${name}*.in 2> /dev/null || true)

if [ -z "${ins}" ]; then
    if [ ${ext} = "sed" ]; then
        echo | ${cmd} > ${tmp}
        perl -i -p0 -e 's/\n\Z//' ${tmp}
    else
        ${cmd} < /dev/null > ${tmp}
    fi
else
    rm -f ${tmp}
    for i in ${ins}; do
        echo "=== ${i} ===" >> ${tmp}
        if [ ${ext} = "ws" ]; then
            (cat ${i} && echo -en "\0") | ${cmd} >> ${tmp}
        elif [ ${ext} = "sed" ]; then
            (cat ${i} && echo) | ${cmd} >> ${tmp}
            perl -i -p0 -e 's/\n\Z//' ${tmp}
        else
            ${cmd} < ${i} >> ${tmp}
        fi
        echo >> ${tmp}
    done
fi

mv ${tmp} ${out}
