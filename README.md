# ELVM Compiler Infrastructure

ELVM is similar to LLVM but dedicated to [Esoteric
Languages](https://esolangs.org/wiki/Main_Page). This project consists
of two components - frontend and backend. Currently, the only frontend
we have is a modified version of
[8cc](https://github.com/rui314/8cc). The modified 8cc translates C
code to an internal representation format called ELVM IR (EIR). Unlike
LLVM bitcode, EIR is designed to be extremely simple, so there's more
chance we can write a translator from EIR to an esoteric language.

Currently, there are 15 backends:

* Ruby
* Python
* JavaScript
* Java
* Emacs Lisp
* Vim script
* Bash
* C
* i386-linux
* C-INTERCAL
* Whitespace
* Befunge
* Brainfuck
* Piet
* Unlambda

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

This backend was contributed by @irori. This backend is even slower
than Brainfuck. See also [8cc.unl](https://github.com/irori/8cc.unl).

This backend is tested with [Emil Jeřábek's
interpreter](http://users.math.cas.cz/~jerabek/unlambda/unl.c). tools/rununl.sh
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

This backend is tested with @koturn's [Whitespace
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

You can run a C compiler on Vim:

* Open test/hello.c (or write your C code)
* `:source /path/to/out/8cc.vim`
* Now you can see ELVM IR in the buffer
* Please prepend a backend name (`vim` for Vim) to the first line
* `:source /path/to/out/elc.vim`
* You can see Vim script code as the compilation result in current buffer
* You can `:source` to run the code

## Future works

I'm interested in

* adding more backends (e.g., WebAssembly, 16bit CPU, Malbolge
  Unshackled, ...)
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
