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
   sp = (sp - 1) & 16777215
   a = 0
   b = sp
   b = bp
   b = (b + 16777215) & 16777215
   a = 4
   mem[b] = a
   a = 42
   b = a
   os.Exit(0)
   os.Exit(0)
  }
  pc++
 }
}
