prof_stack = []
record = []
start = 0

$<.each do |line|
  toks = line.split
  next if toks[0] != 'TRACE' || toks.size != 4

  _, name, time, pc = *toks
  time = time.to_f

  if name =~ /push:(.*)/
    prof_stack << [$1, time, pc]
  elsif name =~ /pop:(.*)/
    name = $1
    next if prof_stack.empty?
    top_name, start, pc = prof_stack.pop
    if top_name != name
      p prof_stack, name
      raise
    end
    record << [name, start, time, pc]
  else
    next
  end
end

while !prof_stack.empty?
  name, start, pc = prof_stack.pop
  record << [name, start, record[-1][2], pc]
end

File.open('trace.json', 'w') do |of|
  a = []
  record.each do |name, st, ed, pc|
    ts = (st - start) * 1000000
    dur = (ed - st) * 1000000
    a << %Q({"cat":"BF","name":"#{name}","ts":#{ts},"dur":#{dur},"tid":1,"pid":1,"args":{"pc":#{pc}},"ph":"X"})
  end
  of.puts("[#{a * ",\n"}]")
end
