def toBinList(b)
  str = ''

  24.times do
    if (b & 1) == 0 then
      str << '0 '
    else
      str << '1 '
    end
    b >>= 1
  end

  return str
end

puts "(define-syntax input!"
puts "  (syntax-rules (quote)"
puts "    ((_ s) (ck s '("

begin
  loop do
    puts ("                   (%s)" % toBinList(STDIN.readbyte))
  end
rescue EOFError
end

puts "                  )))))"
