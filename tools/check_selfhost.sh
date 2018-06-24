#!/bin/bash

if [ -z $1 ]; then
    echo "Usage: $0 <target>"
    exit 1
fi

set -e

TARGET=$1
dir=out/${TARGET}
time=
if [ -e /usr/bin/time ]; then
    time=/usr/bin/time
fi

make out/8cc.c.eir.${TARGET}.out out/elc.c.eir.${TARGET}.out

rm -fr ${dir}
mkdir -p ${dir}/stage1 ${dir}/stage2 ${dir}/stage3

tools/merge_c.rb out/8cc.c > ${dir}/8cc.c
tools/merge_c.rb out/elc.c > ${dir}/elc.c

for prog in 8cc elc; do
    cp out/${prog}.c.eir.${TARGET}* ${dir}/stage1
    sed 's/ *#.*//' out/${prog}.c.eir > ${dir}/stage1/${prog}.c.eir
done

if [ ${TARGET} = x86 ]; then
    run_trg() {
        chmod 755 $1
        ${time} $1
    }
elif [ ${TARGET} = arm ]; then
    run_trg() {
        chmod 755 $1
        ${time} $1
    }
elif [ ${TARGET} = rb ]; then
    run_trg() {
        ${time} ruby $1
    }
elif [ ${TARGET} = ws ]; then
    run_trg() {
        (cat /dev/stdin && echo -ne "\0") | ${time} ./tools/runws.sh $1
    }
elif [ ${TARGET} = bf ]; then
    run_trg() {
        ${time} ./tools/runbf.sh $1
    }
elif [ ${TARGET} = bef ]; then
    run_trg() {
        ${time} ./out/befunge -f $1
    }
elif [ ${TARGET} = unl ]; then
    run_trg() {
        ${time} ./tools/rununl.sh $1
    }
else
    echo "Unknown target: ${TARGET}"
    exit 1
fi

run() {
    run_trg ${dir}/stage$1/$2.c.eir.${TARGET}
}

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

diff -ur ${dir}/stage2 ${dir}/stage3
echo "OK (${TARGET})"
