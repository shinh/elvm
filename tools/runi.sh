#!/bin/sh

if [ ! -e out/ick.flags ]; then
    orig_dir=$(pwd)
    dir=$(mktemp -d)
    cd $dir

    echo 'DO GIVE UP' > do_nothing.i
    ick -Y do_nothing.i > do_nothing.out 2>&1

    echo -n 'ICK_CFLAGS=' > ick.flags
    perl -ple 's/.*?(( -I[^ ]+)+).*/"\1"/' do_nothing.out >> ick.flags
    echo -n 'ICK_LDFLAGS=' >> ick.flags
    perl -ple 's/.*?(( -L[^ ]+)+).*/"\1"/' do_nothing.out >> ick.flags
    echo -n 'ICK_LIBS=' >> ick.flags
    perl -ple 's/.*?(( -l[^ ]+)+).*/"\1"/' do_nothing.out >> ick.flags

    mv ick.flags ${orig_dir}/out
    cd $orig_dir
    rm -fr $dir
fi

. out/ick.flags

ick -b -o $1 > $1.c
tinycc/tcc -Btinycc ${ICK_CFLAGS} ${ICK_LDFLAGS} $1.c ${ICK_LIBS} -o $1.c.exe
$1.c.exe
