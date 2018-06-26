# ELVM Compiler Infrastructure

[![Build Status](https://travis-ci.org/shinh/elvm.svg?branch=master)](http://travis-ci.org/shinh/elvm)

ELVM is similar to LLVM but dedicated to [Esoteric
Languages](https://esolangs.org/wiki/Main_Page). This project consists
of two components - frontend and backend. Currently, the only frontend
we have is a modified version of
[8cc](https://github.com/rui314/8cc). The modified 8cc translates C
code to an internal representation format called
[ELVM IR (EIR)](https://github.com/shinh/elvm/blob/master/ELVM.md).
Unlike LLVM bitcode, EIR is designed to be extremely simple, so
there's more chance we can write a translator from EIR to an esoteric
language.

Currently, there are 37 backends:

* Bash
* Befunge
* Brainfuck
* C
* C++14 constexpr (compile-time) (by [@kw-udon](https://github.com/kw-udon/))
* C++ Template Metaprogramming (compile-time) (by [@kw-udon](https://github.com/kw-udon/)) (WIP)
* C# (by [@masaedw](https://github.com/masaedw/))
* C-INTERCAL
* CommonLisp (by [@youz](https://github.com/youz/))
* Crystal (compile-time) (by [@MakeNowJust](https://github.com/MakeNowJust/))
* Emacs Lisp
* F# (by [@masaedw](https://github.com/masaedw/))
* Forth (by [@dubek](https://github.com/dubek/))
* Go (by [@shogo82148](https://github.com/shogo82148/))
* Java
* JavaScript
* LLVM IR (by [@retrage](https://github.com/retrage/))
* Lua (by [@retrage](https://github.com/retrage/))
* Octave (by [@inaniwa3](https://github.com/inaniwa3/))
* Perl5 (by [@mackee](https://github.com/mackee/))
* PHP (by [@zonuexe](https://github.com/zonuexe/))
* Piet
* Python
* Ruby
* Scheme syntax-rules (by [@zeptometer](https://github.com/zeptometer/))
* SQLite3 (by [@youz](https://github.com/youz/))
* Swift (by [@kwakasa](https://github.com/kwakasa/))
* TeX (by [@hak7a3](https://github.com/hak7a3/))
* TensorFlow (WIP)
* Turing machine (by [@ND-CSE-30151](https://github.com/ND-CSE-30151/))
* Unlambda (by [@irori](https://github.com/irori/))
* Vim script (by [@rhysd](https://github.com/rhysd/))
* WebAssembly (by [@dubek](https://github.com/dubek/))
* Whitespace
* arm-linux (by [@irori](https://github.com/irori/))
* i386-linux
* sed

The above list contains languages which are known to be difficult to
program in, but with ELVM, you can create programs in such
languages. You can easily create Brainfuck programs by writing C code
for example. One of interesting testcases ELVM has is [a tiny Lisp
interpreter](https://github.com/shinh/elvm/blob/master/test/lisp.c). The
all above language backends are passing the test, which means you can
run Lisp on the above languages.

Moreover, 8cc and ELVM themselves are written in C. So we can run a C
compiler written in the above languages to compile the ELVM's compiler
toolchain itself, though such compilation takes long time in some
esoteric languages.

## A demo site

http://shinh.skr.jp/elvm/8cc.js.html

As written, ELVM toolchain itself runs on all supported language
backends. The above demo runs ELVM toolchain on JavaScript (thus slow).

## Example big programs

* [8cc in JavaScript](http://shinh.skr.jp/elvm/8cc.c.eir.js)
* [8cc in Brainfuck](http://shinh.skr.jp/elvm/8cc.c.eir.bf)
* [8cc in Unlambda](http://shinh.skr.jp/elvm/8cc.c.eir.unl)
* [Lisp in Piet](http://shinh.skr.jp/elvm/lisp.png)
* [Lisp in C-INTERCAL](http://shinh.skr.jp/elvm/8cc.c.eir.i)
* [8cc in Befunge](http://shinh.skr.jp/elvm/8cc.c.eir.bef)
* [8cc in Whitespace](http://shinh.skr.jp/elvm/8cc.c.eir.ws)

## ELVM internals

### ELVM IR

* Harvard architecture, not Neumann (allowing self-modifying code is hard)
* 6 registers: A, B, C, D, SP, and BP
* Ops: mov, add, sub, load, store, setcc, jcc, putc, getc, and exit
* Psuedo ops: .text, .data, .long, and .string
* mul/div/mod are implemented by __builtin_*
* No bit operations
* No floating point arithmetic
* sizeof(char) == sizeof(int) == sizeof(void*) == 1
* The word-size is backend dependent, but most backend uses 24bit words
* A single programming counter may contain multiple operations

See [ELVM.md](https://github.com/shinh/elvm/blob/master/ELVM.md) for
more detail.

### Directories

[shinh/8cc's eir branch](https://github.com/shinh/8cc/tree/eir) is the
frontend C compiler.

[ir/](https://github.com/shinh/elvm/tree/master/ir) directory has a
parser and an interpreter of ELVM IR. ELVM IR has

[target/](https://github.com/shinh/elvm/tree/master/target) directory
has backend implementations. Code in this directory uses the IR parser
to generate backend code.

[libc/](https://github.com/shinh/elvm/tree/master/target) directory
has an incomplete libc implementation which is necessary to run
tests.

## Notes on language backends

### Brainfuck

Running a Lisp interpreter on Brainfuck was the first motivation of
this project ([bflisp](https://github.com/shinh/bflisp)). ELVM IR is
designed for Brainfuck but it turned out such a simple IR could be
suitable for other esoteric languages.

As Brainfuck is slow, this project contains a Brainfuck
interpreter/compiler in
[tools/bfopt.cc](https://github.com/shinh/elvm/blob/master/tools/bfopt.cc).
You can also use other optimized Brainfuck implementations such as
[tritium](https://github.com/rdebath/Brainfuck/tree/master/tritium).
Note you need implementations with 8bit cells. For tritium, you need
to specify `-b' flag.

### Unlambda

This backend was contributed by [@irori](https://github.com/irori/).
See also [8cc.unl](https://github.com/irori/8cc.unl).

This backend is tested with [@irori's
interpreter](https://github.com/irori/unlambda). tools/rununl.sh
automatically downloads it.

### C-INTERCAL

This backend uses 16bit registers and address space, though ELVM's
standard is 24bit. Due to the lack of address space, you cannot
compile large C programs using 8cc on C-INTERCAL.

This backend won't be tested by default because C-INTERCAL is slow. Use

    $ CINT=1 make i

to run them. Note you may need to adjust tools/runi.sh.

You can make faster executables by doing something like

    $ cp out/fizzbuzz.c.eir.i fizzbuzz.i && ick fizzbuzz.i
    $ ./fizzbuzz

But compilation takes much more time as it uses gcc instead of tcc.

### Piet

This backend also has 16bit address space. There's the same limitation
as C-INTERCAL's.

This backend won't be tested by default because npiet is slow. Use

    $ PIET=1 make piet

to run them.

### Befunge

[BefLisp](https://github.com/shinh/beflisp), which translates LLVM
bitcode to Befunge, has very similar code. The interpreter,
tools/befunge.cc is mostly Befunge-93, but its address space is
extended to make Befunge-93 Turing-complete.

### Whitespace

This backend is tested with [@koturn](https://github.com/koturn/)'s [Whitespace
implementation](https://github.com/koturn/Whitespace/).

### Emacs Lisp

This backend is somewhat more interesting than other non-esoteric
backends. You can run a C compiler on Emacs:

* M-x load-file tools/elvm.el
* open test/putchar.c (or write C code without #include)
* M-x 8cc
* Now you'll see ELVM IR. You need to prepend a backend name (`el' for
  example) as the first line.
* M-x elc
* M-x eval-buffer
* M-x elvm-main

### Vim script

This backend was contributed by [@rhysd](https://github.com/rhysd/). You can run a C compiler on
Vim:

* Open test/hello.c (or write your C code)
* `:source /path/to/out/8cc.vim`
* Now you can see ELVM IR in the buffer
* Please prepend a backend name (`vim` for Vim) to the first line
* `:source /path/to/out/elc.vim`
* You can see Vim script code as the compilation result in current buffer
* You can `:source` to run the code

You can find more descriptions and released vim script in
[8cc.vim](https://github.com/rhysd/8cc.vim).

### TeX

This backend was contributed by [@hak7a3](https://github.com/hak7a3/). See
also [8cc.tex](https://github.com/hak7a3/8cc.tex).

### C++14 constexpr (compile-time)

This backend was contributed by [@kw-udon](https://github.com/kw-udon/). You can find more
descriptions in
[constexpr-8cc](https://github.com/kw-udon/constexpr-8cc).

### sed

This backend is very slow so only limited tests run by default. You
can run them by

    $ FULL=1 make sed

but it could take years to run all tests. I believe C compiler in sed
works, but I haven't confirmed it's working yet. You can try Lisp
interpreter instead:

    $ FULL=1 make out/lisp.c.eir.sed.out.diff
    $ echo '(+ 4 3)' | time sed -n -f out/lisp.c.eir.sed

This backend should support both GNU sed and BSD sed, so this backend
is more portable than [sedlisp](https://github.com/shinh/sedlisp),
though much slower. Also note, due to limitation of BSD sed, programs
cannot output non-ASCII characters and NUL.

### TensorFlow

Thanks to control flow operations such as tf.while_loop and tf.cond,
a TensorFlow's graph is Turing complete. This backend translates EIR
to a Python code which constructs a graph which is equivalent to the
source EIR. This backend is very slow and uses a huge amount of
memory. I've never seen 8cc.c.eir.tf works, but lisp.c.eir.tf does
work. You can test this backend by

    $ TF=1 make tf

TODO: Reduce the size of the graph and run 8cc

## Future works

I'm interested in

* adding more backends (e.g., 16bit CPU, Malbolge Unshackled, ...)
* running more programs (e.g., lua.bf or mruby.bf?)
* supporting more C features (e.g., bit operations)
* eliminating unnecessary code in 8cc

Adding a backend shouldn't be extremely difficult. PRs are welcomed!

## See also

This project is a sequel of [bflisp](https://github.com/shinh/bflisp).

* [Lisp in sed](https://github.com/shinh/sedlisp)
* [Lisp in Befunge](https://github.com/shinh/beflisp)
* [Lisp in GNU make](https://github.com/shinh/makelisp)
* [Slide deck in Japanese](http://shinh.skr.jp/slide/elvm/000.html)

## Acknowledgement

I'd like to thank [Rui Ueyama](https://github.com/rui314/) for his
easy-to-hack compiler and suggesting the basic idea which made this
possible.
