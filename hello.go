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
  72,
  101,
  108,
  108,
  111,
  44,
  32,
  119,
  111,
  114,
  108,
  100,
  33,
  10,
  0,
  16,
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
   a = 0
   b = bp
   b = (b + 16777215) & 16777215
   mem[b] = a

  case 2:
   b = bp
   b = (b + 16777215) & 16777215
   a = mem[b]
   b = a
   a = mem[b]
   if a == 0 { pc = 4 - 1 }

  case 3:
   if true { pc = 5 - 1 }

  case 4:
   if true { pc = 7 - 1 }

  case 5:
   b = bp
   b = (b + 16777215) & 16777215
   a = mem[b]
   b = a
   a = mem[b]
   d = sp
   d = (d + 16777215) & 16777215
   mem[d] = a
   sp = d
   buf[0] = byte(a); os.Stdout.Write(buf[:])
   sp = (sp + 1) & 16777215

  case 6:
   b = bp
   b = (b + 16777215) & 16777215
   a = mem[b]
   d = sp
   d = (d + 16777215) & 16777215
   mem[d] = a
   sp = d
   a = (a + 1) & 16777215
   b = bp
   b = (b + 16777215) & 16777215
   mem[b] = a
   a = mem[sp]
   sp = (sp + 1) & 16777215
   if true { pc = 2 - 1 }

  case 7:
   a = 0
   b = a
   os.Exit(0)
   os.Exit(0)
  }
  pc++
 }
}
