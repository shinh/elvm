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

Currently, there are 60 backends:

1. *Acc!!*
1. Aheui
1. Awk (by [@dubek](https://github.com/dubek/))
1. Bash
1. Befunge
1. Binary Lambda Calculus (by [@woodrush](https://github.com/woodrush/))
1. Brainfuck
1. C
1. C++14 constexpr (compile-time) (by [@kw-udon](https://github.com/kw-udon/))
1. C++ Template Metaprogramming (compile-time) (by [@kw-udon](https://github.com/kw-udon/)) (WIP)
1. C# (by [@masaedw](https://github.com/masaedw/))
1. C-INTERCAL
1. CMake (by [@ooxi](https://github.com/ooxi/))
1. CommonLisp (by [@youz](https://github.com/youz/))
1. Conway's Game of Life (via QFTASM) (by [@woodrush](https://github.com/woodrush/))
1. Crystal (compile-time) (by [@MakeNowJust](https://github.com/MakeNowJust/))
1. Emacs Lisp
1. F# (by [@masaedw](https://github.com/masaedw/))
1. Forth (by [@dubek](https://github.com/dubek/))
1. Fortran (by [@samcoppini](https://github.com/samcoppini/))
1. Go (by [@shogo82148](https://github.com/shogo82148/))
1. Go text/template (Gomplate) (by [@Syuparn](https://github.com/syuparn/))
1. Grass (by [@woodrush](https://github.com/woodrush/))
1. HeLL (by [@esoteric-programmer](https://github.com/esoteric-programmer/))
1. J (by [@dubek](https://github.com/dubek/))
1. Java
1. JavaScript
1. Kinx (by [@Kray-G](https://github.com/Kray-G/))
1. Lambda calculus (by [@woodrush](https://github.com/woodrush/))
1. Lazy K (by [@woodrush](https://github.com/woodrush/))
1. LLVM IR (by [@retrage](https://github.com/retrage/))
1. LOLCODE (by [@gamerk](https://github.com/gamerk))
1. Lua (by [@retrage](https://github.com/retrage/))
1. Octave (by [@inaniwa3](https://github.com/inaniwa3/))
1. Perl5 (by [@mackee](https://github.com/mackee/))
1. PHP (by [@zonuexe](https://github.com/zonuexe/))
1. Piet
1. Python
1. Ruby
1. Scheme syntax-rules (by [@zeptometer](https://github.com/zeptometer/))
1. Scratch3.0 (by [@algon-320](https://github.com/algon-320/))
1. SQLite3 (by [@youz](https://github.com/youz/))
1. SUBLEQ (by [@gamerk](https://github.com/gamerk))
1. Swift (by [@kwakasa](https://github.com/kwakasa/))
1. Tcl (by [@dubek](https://github.com/dubek/))
1. TeX (by [@hak7a3](https://github.com/hak7a3/))
1. TensorFlow (WIP)
1. Turing machine (by [@ND-CSE-30151](https://github.com/ND-CSE-30151/))
1. Unlambda (by [@irori](https://github.com/irori/))
1. Universal Lambda (by [@woodrush](https://github.com/woodrush/))
1. Vim script (by [@rhysd](https://github.com/rhysd/))
1. WebAssembly (by [@dubek](https://github.com/dubek/))
1. WebAssembly System Interface (by [@sanemat](https://github.com/sanemat/))
1. Whirl by ([@samcoppini](https://github.com/samcoppini/))
1. W-Machine by ([@jcande](https://github.com/jcande/))
1. Whitespace
1. arm-linux (by [@irori](https://github.com/irori/))
1. i386-linux
1. sed

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

### HeLL

This backend was contributed by [@esoteric-programmer](https://github.com/esoteric-programmer/).
HeLL is an assembly language for Malbolge and Malbolge Unshackled.
Use [LMFAO](https://github.com/esoteric-programmer/LMFAO) to build the Malbolge Unshackled program from HeLL.
This backend won't be tested by default because Malbolge Unshackled is extremely slow. Use

    $ HELL=1 make hell

to run them. Note you may need to adjust tools/runhell.sh.

This backend does not support all 8-bit characters on I/O, because I/O of Malbolge Unshackled
uses Unicode codepoints instead of single bytes in getc/putc calls.
Further, the Malbolge Unshackled interpreter automatically converts newlines read from stdin,
which cannot be revert in a platform independent way.
The backend reverts/converts newlines from input to Linux encoding and
applies modulo 256 operations to all input and output,
but it cannot compensate the issues this way.
You should limit I/O to ASCII characters in order to avoid unexpected behaviour or crashes.

This backend may be replaced by a Malbolge Unshackled backend in the future.

### TensorFlow

Thanks to control flow operations such as tf.while_loop and tf.cond,
a TensorFlow's graph is Turing complete. This backend translates EIR
to a Python code which constructs a graph which is equivalent to the
source EIR. This backend is very slow and uses a huge amount of
memory. I've never seen 8cc.c.eir.tf works, but lisp.c.eir.tf does
work. You can test this backend by

    $ TF=1 make tf

TODO: Reduce the size of the graph and run 8cc

### Scratch 3.0

[Scratch](https://scratch.mit.edu/) is a visual programming language.

Internally, a Scratch program consists of a JSON that represent the program and some resources such as 
images or sounds.
They are zip-archived and you can import/export them from project page (Create new one from [here](https://scratch.mit.edu/projects/editor/)).

You can use `tools/gen_scratch_sb3.sh` to generate complete project files from output of this backend,
and `tools/run_scratch.js` to execute programs from command line (npm 'scratch-vm' package is required).

You can try "fizzbuzz_fast" sample from [here](https://scratch.mit.edu/projects/383294522/).

#### Example (for `test/basic.eir`)
First, generate scratch project.
```sh
$ ./out/elc -scratch3 test/basic.eir > basic.scratch3
$ ./tools/gen_scratch_sb3.sh basic.scratch3
$ ls basic.scratch3.sb3
basic.scratch3.sb3
```

##### Execute it from Web browser
1. Visit [https://scratch.mit.edu/projects/editor](https://scratch.mit.edu/projects/editor).
2. Click a menu item: "File".
3. Click "Load from your computer".
4. Select and upload the generated project file: `basic.scratch3.sb3`.
5. Wait until the project is loaded. (It takes a long time for a hevy project.)
6. Click the "Green Flag"

From the Web editor, to input special characters (LF, EOF, etc.) you have to input them explicitly by following:
|special character|representation|
|-----------------|--------------|
| LF              |`＼n`         |
| EOF             |`＼0`         |
| other character with codepoint XXX (decimal) |`＼dXXX`|

Note that: the escape character is `＼` (U+FF3C) not `\`.

For normal ASCII characters, you can just put them into the input field.

##### Execute it from command line
1. First install the npm package ["scratch-vm"](https://github.com/LLK/scratch-vm) under the `tools` directory :
```sh
$ cd tools
$ npm install scratch-vm
```
2. Run it with `tools/run_scratch.js`:
```
$ echo -n '' | nodejs ./run_scratch.js ../basic.scratch3.sb3
!!@X
```

### Conway's Game of Life

This backend was contributed by [@woodrush](https://github.com/woodrush/) based on [QFTASM](https://github.com/QuestForTetris/QFT).
See [tools/qftasm/README.md](tools/qftasm/README.md) for its details.
Further implementation details are described in the [Lisp in Life](https://github.com/woodrush/lisp-in-life) project.

### Binary Lambda Calculus
This backend was contributed by [@woodrush](https://github.com/woodrush/).
Implementation details are described in the [LambdaVM](https://github.com/woodrush/lambdavm) and [lambda-8cc](https://github.com/woodrush/lambda-8cc) repositories.

The output of this backend is an untyped lambda calculus term written in [binary lambda calculus](https://tromp.github.io/cl/Binary_lambda_calculus.html) notation.
The output program runs on the [IOCCC](https://www.ioccc.org/) 2012 ["Most Functional"](https://www.ioccc.org/2012/tromp/hint.html) interpreter written by [@tromp](https://github.com/tromp).
The program runs on the byte-oriented mode which is the default mode.

This backend outputs a sequence of 0/1s written in ASCII.
This bit stream must be packed into a byte stream before passing it to the interpreter,
which can be done using tools/packbits.c. Please see tools/runblc.sh for usage details.

This backend is tested with the interpreter [uni](https://github.com/melvinzhang/binary-lambda-calculus),
a fast implementation of the "Most Functional" interpreter written in C++ by [@melvinzhang](https://github.com/melvinzhang).
This interpreter significantly speeds up the running time of large programs such as 8cc.c.
tools/runblc.sh automatically clones and builds uni via tools/runblc.sh when the tests are run.

### Lambda Calculus
This backend was contributed by [@woodrush](https://github.com/woodrush/).
This backend outputs an untyped lambda calculus term written in plain text, such as `\x.(x x)`.

The I/O model used in this backend is identical to the one used in the [Binary Lambda Calculus backend](#binary-lambda-calculus).
The backend's output program is a lambda calculus term that takes a string as an input and returns a string.
Here, strings are encoded into lambda calculus terms using Scott encoding and Church encoding,
so the entire computation only consists of the beta-reduction of lambda calculus terms.
Further implementation details are described in the [LambdaVM](https://github.com/woodrush/lambdavm) and [lambda-8cc](https://github.com/woodrush/lambda-8cc) repositories.
Note that the backend's output program is assumed to be evaluated using a lazy evaluation strategy.

This backend is tested with the interpreter [uni](https://github.com/melvinzhang/binary-lambda-calculus),
written by [@melvinzhang](https://github.com/melvinzhang).
The [blc](https://github.com/tromp/AIT) tool written by [@tromp](https://github.com/tromp) is also used to convert plain text lambdas into binary lambda calculus notation, the format accepted by `uni`.
Both tools are automatically cloned and built via tools/runlam.sh when the tests are run.


### Lazy K
The [Lazy K](https://tromp.github.io/cl/lazy-k.html) backend was contributed by [@woodrush](https://github.com/woodrush/).
Implementation details are described in the [LambdaVM](https://github.com/woodrush/lambdavm) and [lambda-8cc](https://github.com/woodrush/lambda-8cc) repositories.

This backend is tested with the Lazy K interpreter [lazyk](https://github.com/irori/lazyk) written by [@irori](https://github.com/irori).
Interactive programs require the `-u` option which disables standard output buffering, used as `lazyk -u [input file]`.
The interpreter is automatically cloned and built via tools/runlazy.sh when the tests are run.

### Universal Lambda
The [Universal Lambda](http://www.golfscript.com/lam/) backend was contributed by [@woodrush](https://github.com/woodrush/).
Implementation details are described in the [LambdaVM](https://github.com/woodrush/lambdavm) repository.

This backend is tested with the Universal Lambda interpreter [clamb](https://github.com/irori/clamb) written by [@irori](https://github.com/irori).
Interactive programs require the `-u` option which disables standard output buffering, used as `clamb -u [input file]`.
The interpreter is automatically cloned and built via tools/runulamb.sh when the tests are run.

The output of this backend is an untyped lambda calculus term written in the [binary lambda calculus](https://tromp.github.io/cl/Binary_lambda_calculus.html) notation.
The output program is written as a sequence of 0/1s in ASCII.
The bit stream must be packed into a byte stream before passing it to the interpreter.
This can be done using tools/packbits.c. Please see tools/runulamb.sh for usage details.

### Grass
The [Grass](http://www.blue.sky.or.jp/grass/) backend was contributed by [@woodrush](https://github.com/woodrush/).
Implementation details are described in the [GrassVM](https://github.com/woodrush/grassvm) and [LambdaVM](https://github.com/woodrush/lambdavm) repositories.

This backend is tested with the Grass interpreter [grass.ml](https://gist.github.com/woodrush/3d85a6569ef3c85b63bfaf9211881af6), originally written by [@ytomino](https://github.com/ytomino) and modified by [@youz](https://github.com/youz) and [@woodrush](https://github.com/woodrush).
The modifications are described in the [GrassVM](https://github.com/woodrush/grassvm) repository.


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
