* 参考
  * https://rhysd.hatenablog.com/entry/2016/12/02/030511
  * 

* 手順諸々
  * 各種バックエンド用のコードを生成する
    * 1. C -> eirへコンパイル
      * `./out/8cc -S -fdump-ast -I. -Ilibc -o hello.eir hello.c`
    * 2. eir -> バックエンド言語
      * `./out/elc -(Targets) hello.eir > hello.targets`
        * target名はMakefileに記載されてる.

12/2
* とりあえずmakeして、cコードをGoコードにcompileして動きを確かめた

12/8
* 今日の目標: Goを例にとってelvm irの理解(https://github.com/shinh/elvm/commits?author=shogo82148)
* なんかeirが吐くコードがよくわかんないから、8ccから読んでいくアプローチ人してみる
  * 記事調べても、バックエンド実装してみた系敷かないし。。。

12/11
* 8ccのcodegenの部分を読んでみる
  * d

12/12
* 実はCのincludeが分かってない説
  * libcを自前で実装してるみたいだが、putcharはどこからincludeされるのか、よくわからなかった
  * あ、putcがオレオレCPU命令なのか
    * emit_func_call()関数を参照
  * 多分、putc(printする系全部)に依存してるlibcの関数を再実装したんだと思う。