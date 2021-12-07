* コメント付きgo file
```
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
 mem := make([]int32, 1<<24) // なんで24なんだろう.
 copy(mem, []int32{
  1,
 })

 for {
  switch pc {

  case 0:
   if true { pc = 1 - 1 }

  case 1:
   d = sp
   d = (d + 16777215) & 16777215 // 	add D, -1
   mem[d] = bp
   sp = d   // spここで代入されてる
   bp = sp // bpここで入れられてる.
   sp = (sp - 2) & 16777215
   a = 0
   b = sp
   b = bp
   b = (b + 16777215) & 16777215
   a = 4
   mem[b] = a
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
   b = bp   // 	mov B, BP
   b = (b + 16777214) & 16777215    	// add B, 16777214
   a = mem[b]               	// load A, B
   b = a                        	// mov B, A
   a = mem[sp]              //  	load A, SP
   sp = (sp + 1) & 16777215 // 	add SP, 1
   a = (a + b) & 16777215 // 	add A, B
   b = a
   os.Exit(0)
   os.Exit(0)
  }
  pc++
 }
}
```