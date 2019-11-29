#!/bin/bash

# Usage: split_mcfunctions.sh <joined_src> <out_dir>

set -e

rm -rf $2

FILE=""

while IFS="" read -r l || [ -n "$l" ]
do
    if [[ ${#l} -gt 0 && "${l:0:1}" == "=" ]];
    then
        FILE="$(echo "$l" | sed -r -e "s/========= (.*?):(.*?) =========/$(echo "$2" | sed -e 's/\//\\\//g')\/\1\/functions\/\2/g")"
        mkdir -p $(dirname "$FILE")
    else
        echo "$l" >> ${FILE}
    fi
done < $1
