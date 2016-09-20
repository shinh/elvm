#!/usr/bin/env ruby

require 'fileutils'
require 'json'

system("make out/8cc.c.eir.js out/elc.c.eir.js")

headers = {}
Dir.glob('libc/*.h').each do |hf|
  h = File.read(hf)
  hn = File.basename(hf)
  headers[hn] = h
end

FileUtils.mkdir_p('web')
FileUtils.ln_sf('../tools/8cc.js.html', 'web')
FileUtils.ln_sf('../out/8cc.c.eir.js', 'web')
FileUtils.ln_sf('../out/elc.c.eir.js', 'web')
File.open('web/headers.js', 'w') do |of|
  of.print 'var HEADERS = '
  of.print JSON.dump(headers)
  of.puts ';'
end
