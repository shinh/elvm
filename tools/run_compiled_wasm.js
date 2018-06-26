#!/usr/bin/env nodejs

const fs = require('fs');
const util = require('util');
const readFile = util.promisify(fs.readFile);

var input = Buffer.from([]);
var ip = 0;

process.stdin.on('data', (chunk) => {
  input = Buffer.concat([input, chunk])
});

const env = {
  getchar: function() {
    return input[ip++] | 0;
  },

  putchar: function(c) {
    process.stdout.write(String.fromCharCode(c & 255));
  },

  exit: function() {
    process.exit(0);
  }
}

async function runWasm(filename) {
  try {
    const buf = await readFile(filename);
    const module = await WebAssembly.compile(buf);
    const instance = new WebAssembly.Instance(module, {env: env});
    instance.exports.wasmmain();
    process.exit(0);
  } catch (e) {
    process.stderr.write("ERROR: " + e)
    process.exit(1);
  }
}

runWasm(process.argv[2]);
