#!/usr/bin/env ruby

require 'fileutils'
require 'json'

system("make out/8cc.c.eir.asmjs out/elc.c.eir.asmjs out/eli.c.eir.asmjs")

headers = {}
Dir.glob('libc/*.h').each do |hf|
  h = File.read(hf)
  hn = File.basename(hf)
  headers[hn] = h
end

FileUtils.mkdir_p('web')
FileUtils.ln_sf('../tools/8cc.js.html', 'web')
FileUtils.ln_sf('../out/8cc.c.eir.asmjs', 'web/8cc.c.eir.js')
FileUtils.ln_sf('../out/elc.c.eir.asmjs', 'web/elc.c.eir.js')
FileUtils.ln_sf('../out/eli.c.eir.asmjs', 'web/eli.c.eir.js')
File.open('web/headers.js', 'w') do |of|
  of.print 'var HEADERS = '
  of.print JSON.dump(headers)
  of.puts ';'
end
