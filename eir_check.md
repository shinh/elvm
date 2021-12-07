elvmのeir形式についてみていく

### case1
  * input
```
int main() {
  return 42;
}
```
* out
```
	.text
main:
	#{push:main}
	mov D, SP
	add D, -1
	store BP, D
	mov SP, D
	mov BP, SP
	.file 1 "hello.c"
	.loc 1 4 0
	# }
	mov A, 42
	mov B, A
	#{pop:main}
	exit
	#{pop:main}
	exit

```

* memo:
  * #は多分コメント
  * pop:mainから始まるのが2つあるけど、1つ消しても動いた
  * Bがrax的なものかな？(関数抜ける時に値をおいておく場所)
  * なんか、BP = SP になってる気しかしない。。。

### case2
* in
```
int main() {
  int a = 4;
  return 42;
}
```

* out
```
	.text
main:
	#{push:main}
	mov D, SP
	add D, -1
	store BP, D
	mov SP, D
	mov BP, SP
	sub SP, 1
	.file 1 "hello.c"
	.loc 1 5 0
	# }
	.loc 1 4 0
	#   return 42;
	mov A, 0
	mov B, SP
	mov B, BP
	add B, 16777215
	mov A, 4
	store A, B
	.loc 1 5 0
	# }
	mov A, 42
	mov B, A
	#{pop:main}
	exit
	#{pop:main}
	exit

```
* memo:
  * .loc xxxx....ってやつ、デバッグ情報だわ。。
  * 16,777,215って数値、UINT_MAX_STRらしい(2^24 - 1)
    * elvmだと24bitが1wordって書いてあったな。
    * bit演算とかする際に、2^n - 1がいろんな場面で使えるみたい
  * add B, 16777215は何を意味するのだろう..
  * store命令のdstにはaddress(or regiseter)がきて、そのアドレスの中に値をいれるって認識でいいのか？

### case3
* in
```
int main() {
  int a = 4;
  int b = 33;
  return 42;
}
```
* out
```
	.text
main:
	#{push:main}
	mov D, SP
	add D, -1
	store BP, D
	mov SP, D
	mov BP, SP # (ここまでで　SP == BP)
	# MEMO: ここまではroutineっぽい
	sub SP, 2 #ここがlocal変数依存. spからword数をひく. (SP != BP)
	.file 1 "hello.c"
	.loc 1 6 0
	# }
	.loc 1 4 0
	mov A, 0
	mov B, SP 
	mov B, BP
	add B, 16777215
	mov A, 4
	store A, B
	.loc 1 5 0
	mov A, 0
	mov B, SP
	mov B, BP
	add B, 16777214
	mov A, 33
	store A, B
	.loc 1 6 0
	mov A, 42
	mov B, A
	#{pop:main}
	exit
	#{pop:main}
	exit
```
* memo
  * locとcommentの対応が合ってないような？？
  * 	.file 1 "hello.c" とかも、多分debug情報かな. 
  * a, bどっちも同じようなルーチンで保存してるけど、これ後に宣言した方が先に宣言したものを上書きしてない？
    * return a + b;をして確認してみる
  * a

### case4
* in 
```
int main() {
  int a = 4;
  int b = 33;
  return a + b;
}
```
* out
```
	.text
main:
	#{push:main}
	mov D, SP
	add D, -1
	store BP, D
	mov SP, D
	mov BP, SP
	sub SP, 2
	.file 1 "hello.c"
	.loc 1 6 0
	# }
	#############
	.loc 1 4 0
	mov A, 0
	mov B, SP
	mov B, BP
	add B, 16777215
	mov A, 4
	store A, B
	#############
	.loc 1 5 0
	mov A, 0
	mov B, SP
	mov B, BP
	add B, 16777214
	mov A, 33
	store A, B
	#############
	.loc 1 6 0
	# }
	.loc 1 4 0
	mov B, BP
	add B, 16777215
	load A, B
	mov D, SP
	add D, -1
	store A, D
	mov SP, D
	.loc 1 5 0
	mov B, BP
	add B, 16777214
	load A, B
	mov B, A
	load A, SP
	add SP, 1
	add A, B
	mov B, A
	#{pop:main}
	exit
```

* memo
  * goコードにfmtとか仕込んでデバッグするの良さげ
  * `add B (16777215 - n)` ってのが、`sub B - (n + 1)` に対応するみたい.
  * goのコードみるからに、bはmemのindex(memory address)を表現してるみたい.
  * データはメモリの後ろ(mem[b]のbが大きい値)から配置されるみたい
  * bpとspについて
    * 最初のroutineでbpとspはどっちも16777215にセットされるみたい
    * で、spは
      * local変数の数nに応じて、nだけ下げられる

### case5
* in
```
int main() {
  int a = 4;
  a += 1;
  int b = 33;
  return a + b;
}
```

* out 
```
	.text
main:
	#{push:main}
	mov D, SP
	add D, -1
	store BP, D
	mov SP, D
	mov BP, SP
	sub SP, 2
	.file 1 "hello.c"
	.loc 1 7 0
	# }
	.loc 1 4 0
	#   a += 1;
	mov A, 0
	mov B, SP
	mov B, BP
	add B, 16777215
	mov A, 4
	store A, B


	.loc 1 4 0
	#   a += 1;
	mov B, BP
	add B, 16777215
	load A, B
	mov D, SP
	add D, -1
	store A, D
	mov SP, D

	######################

	.loc 1 5 0
	#   int b = 33;
	mov A, 1
	mov B, A
	load A, SP
	add SP, 1
	add A, B
	mov B, BP
	add B, 16777215
	store A, B

	######################

	.loc 1 6 0
	mov A, 0
	mov B, SP
	mov B, BP
	add B, 16777214
	mov A, 33
	store A, B

	
	.loc 1 7 0
	mov B, BP
	add B, 16777215
	load A, B
	mov D, SP
	add D, -1
	store A, D
	mov SP, D
	.loc 1 6 0
	#   return a + b;
	mov B, BP
	add B, 16777214
	load A, B
	mov B, A
	load A, SP
	add SP, 1
	add A, B
	mov B, A
	#{pop:main}
	exit
	#{pop:main}
	exit

```

* go code (with memo)
```
/*
input::

int main() {
  int a = 4;
  a += 1;
  int b = 33;
  return a + b;
}

*/

package main
import "os"
func main() {
 var a int32
 _ = a
 var b int32
 _ = b
 var c int32
 _ = c
 var d int32
 _ = d
 var bp int32
 _ = bp
 var sp int32
 _ = sp
 var pc int32
 _ = pc
 var buf [1]byte
 _ = buf
 mem := make([]int32, 1<<24)
 copy(mem, []int32{
  1,
 })

 for {
  switch pc {

  case 0:
   if true { pc = 1 - 1 }

  case 1:
   d = sp
   d = (d + 16777215) & 16777215
   mem[d] = bp
   sp = d
   bp = sp
   sp = (sp - 2) & 16777215
   a = 0
   b = sp // b == 16777215
   b = bp
   b = (b + 16777215) & 16777215 // b == 16777214
   a = 4
   mem[b] = a   // mem[16777214] = 4

   // 	.loc 1 4 0
   b = bp // b = 16777215
   b = (b + 16777215) & 16777215 // b = 16777214
   a = mem[b] // a = mem[16777214] = 4
   d = sp   // d = 16777215
   d = (d + 16777215) & 16777215 // d = 16777214
   mem[d] = a   // mem[16777214] = 4 = a
   sp = d       // sp = 16777214 //  @@@@@@ ここでspを変えてしまうのがミソ @@@@@@@

   // .loc 1 5 0
   a = 1        // a = 1
   b = a        // b = a = 1
   a = mem[sp]  // a = mem[16777214]
   sp = (sp + 1) & 16777215 // spを元に戻してる?? sp = 16777215
   a = (a + b) & 16777215
   b = bp
   b = (b + 16777215) & 16777215
   mem[b] = a

   // 	.loc 1 6 0
   a = 0
   b = sp
   b = bp
   b = (b + 16777214) & 16777215
   a = 33
   mem[b] = a
   b = bp
   b = (b + 16777215) & 16777215
   a = mem[b]
   d = sp
   d = (d + 16777215) & 16777215
   mem[d] = a
   sp = d
   b = bp
   b = (b + 16777214) & 16777215
   a = mem[b]
   b = a
   a = mem[sp]
   sp = (sp + 1) & 16777215
   a = (a + b) & 16777215
   b = a
   os.Exit(0)
   os.Exit(0)
  }
  pc++
 }
}

```

* memo
  * 