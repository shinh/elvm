// This script executes a Scratch 3.0 program that the Scratch backend generates.
// To run this script, first install npm package 'scratch-vm' under 'tools' directory (here)
// by the following command:
//     $ ls run_scratch.js # make sure you are in the same directory
//     run_scratch.js
//     $ npm install scratch-vm  # this generates 'node_modules' on this directory.

const fs = require('fs');

const VirtualMachine = require('scratch-vm');
const vm = new VirtualMachine();
vm.start();
vm.clear();
vm.setCompatibilityMode(false);
vm.setTurboMode(true);

// Ignore warning / error messages
require('minilog').disable();

// Replace special characters with '＼dXXX'
const special_chars = /([^ !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~])/g;
const inputStr = fs.readFileSync(0).toString().
    replace(special_chars, (_match, g, _offset, _str) => '＼d' + g.charCodeAt(0).toString().padStart(3, '0'));

// Input bytes from STDIN as a response to Scratch's ask block.
// If the program requires more input, put EOF ('＼0').
let inputDone = false;
vm.runtime.addListener('QUESTION', q => {
    if (q === 'stdin' && !inputDone) {
        vm.runtime.emit('ANSWER', inputStr);
        inputDone = true;
    } else {
        vm.runtime.emit('ANSWER', '＼0'); // put EOF
    }
});

// Get output bytes that the program wrote as contents of Scratch's list block,
// then replace special characters with corresponding ones.
// ('＼dXXX' (decimal XXX) → a character with code point XXX)
function printResult() {
    const stdout_result = vm.runtime.targets[0].variables['#l:stdout'].value;
    const stdout_str =
        stdout_result.join('\n').replace(/＼(\d{1,3})/g,
            (_match, g, _offset, _str) => String.fromCharCode(g));
    process.stdout.write(stdout_str);
    process.nextTick(process.exit);
};

// Count active threads and detect finish of execution.
function whenThreadsComplete() {
    return new Promise((resolve, reject) => {
        setInterval(() => {
            let active = 0;
            const threads = vm.runtime.threads;
            for (let i = 0; i < threads.length; i++) {
                if (!threads[i].updateMonitor) {
                    active += 1;
                }
            }
            if (active === 0) {
                resolve();
            }
        }, 100);
    })
}

const filename = process.argv[2];
const project = new Buffer(fs.readFileSync(filename));

vm.loadProject(project)
    .then(() => vm.greenFlag())
    .then(() => whenThreadsComplete())
    .then(printResult);
