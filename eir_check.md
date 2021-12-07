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