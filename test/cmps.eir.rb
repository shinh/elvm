def emit_print(m)
  m.each_byte{|b|
    puts "mov A, #{b}"
    puts "putc A"
  }
end

CMP_INSTS = %w(eq ne lt gt le ge)

6.times do |i|
  inst = CMP_INSTS[i]
  emit_print(inst + ": ")
  [[999, 1000], [1000, 1000], [1001, 1000]].each do |lhs, rhs|
    puts "mov A, #{lhs}"
    puts "#{inst} A, #{rhs}"
    puts "add A, 48"
    puts "putc A"
  end
  emit_print "\n"
end
puts "exit"
