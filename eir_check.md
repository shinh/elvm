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
  * add B, 16777215は何を意味するのだろう..
  * store命令のdstにはaddress(or regiseter)がきて、そのアドレスの中に値をいれるって認識でいいのか？