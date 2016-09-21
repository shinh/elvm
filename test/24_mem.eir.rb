10.times{|i|
  puts %Q(mov C, #{65530+i}
mov B, #{42+i}
store B, C
putc B
load A, C
putc A
)
}
puts 'exit'
