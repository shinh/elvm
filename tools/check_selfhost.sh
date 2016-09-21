#!/bin/bash

if [ -z $1 ]; then
    echo "Usage: $0 <target>"
    exit 1
fi

set -e

TARGET=$1
dir=out/${TARGET}

make out/8cc.c.eir.${TARGET}.out

rm -fr ${dir}
mkdir -p ${dir}/stage1 ${dir}/stage2 ${dir}/stage3

tools/merge_c.rb out/8cc.c > ${dir}/8cc.c
tools/merge_c.rb out/elc.c > ${dir}/elc.c

for prog in 8cc elc; do
    cp out/${prog}.c.eir.${TARGET}* ${dir}/stage1
    sed 's/ *#.*//' out/${prog}.c.eir > ${dir}/stage1/${prog}.c.eir
done

if [ ${TARGET} = x86 ]; then
    run() {
        local exe=${dir}/stage$1/$2.c.eir.x86
        chmod 755 ${exe}
        /usr/bin/time ${exe}
    }
elif [ ${TARGET} = rb ]; then
    run() {
        /usr/bin/time ruby ${dir}/stage$1/$2.c.eir.rb
    }
else
    echo "Unknown target: ${TARGET}"
    exit 1
fi

for stage in 1 2; do
    nstage=$((${stage} + 1))
    for prog in 8cc elc; do
        echo "Building stage${nstage} ${prog}.eir"
        cat ${dir}/${prog}.c | run ${stage} 8cc > ${dir}/stage${nstage}/${prog}.c.eir
        echo "Building stage${nstage} ${prog}.${TARGET}"
        (echo ${TARGET} && cat ${dir}/stage${nstage}/${prog}.c.eir) | \
            run ${stage} elc > ${dir}/stage${nstage}/${prog}.c.eir.${TARGET}
    done
done

diff -ur out/x86/stage2 out/x86/stage3
