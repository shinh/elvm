from pyparsing import *
import sys

#==================================================================================
# Configurations
#==================================================================================

QFTASM_RAM_AS_STDIN_BUFFER = True
QFTASM_RAM_AS_STDOUT_BUFFER = True

QFTASM_RAMSTDIN_BUF_STARTPOSITION = 7167
QFTASM_RAMSTDOUT_BUF_STARTPOSITION = 8191

debug_ramdump = False
debug_plot_memdist = False   # Requires numpy and matplotlib when set to True
use_stdio = True
stdin_from_pipe = True

#==================================================================================
QFTASM_STDIO_OPEN = 1 << 8
QFTASM_STDIO_CLOSED = 1 << 9
QFTASM_STDIN = 1
QFTASM_STDOUT = 2

class Parser(object):
    def __init__(self):
        addressing_mode = Optional(Char("ABC")).setParseAction(
            lambda t: {"A": 1, "B": 2, "C": 3}[t[0]] if len(t) > 0 else 0
        )
        integer = (Optional("-") + Word(nums)).setParseAction(
            lambda t: int("".join(t))
        )
        operand = Group(addressing_mode + integer)
        MNZ = Literal("MNZ")
        MLZ = Literal("MLZ")
        ADD = Literal("ADD")
        SUB = Literal("SUB")
        AND = Literal("AND")
        OR = Literal("OR")
        XOR = Literal("XOR")
        ANT = Literal("ANT")
        SL = Literal("SL")
        SRL = Literal("SRL")
        SRA = Literal("SRA")
        opcode = MNZ | MLZ | ADD | SUB | AND | OR | XOR | ANT | SL | SRL | SRA
        lineno = (integer + Literal(".")).suppress()
        comment = (Literal(";") + restOfLine).suppress()
        inst = (lineno + opcode + operand + operand + operand + Optional(comment)).setParseAction(
            lambda t: [t]
        )
        self.program = ZeroOrMore(inst)

    def parse_string(self, string):
        return self.program.parseString(string)

def interpret_file(filepath):
    def wrap(x):
        return x & ((1 << 16) - 1)

    d_inst = {
        "MNZ": lambda a, b, c: (True, b, c) if a != 0 else (False, None, None),
        "MLZ": lambda a, b, c: (True, b, c) if (wrap(a) >> 15) == 1 else (False, None, None),
        "ADD": lambda a, b, c: (True, wrap(a+b), c),
        "SUB": lambda a, b, c: (True, wrap(a-b + (1 << 16)), c),
        "AND": lambda a, b, c: (True, (a & b), c),
        "OR" : lambda a, b, c: (True, (a | b), c),
        "XOR": lambda a, b, c: (True, (a ^ b), c),
        "ANT": lambda a, b, c: (True, (a & (~b)), c),
        "SL" : lambda a, b, c: (True, (a << b), c),
        "SRL": lambda a, b, c: (True, (a >> b), c),
        "SRA": lambda a, b, c: (True, (a & (1 << 7)) ^ (a & ((1 << 7) - 1) >> b), c),
    }

    parser = Parser()

    with open(filepath, "rt") as f:
        rom_lines = f.readlines()
    rom = rom_lines
    ram = []
    for _ in range(1 << 16):
        ram.append([0,0])

    if use_stdio:
        stdin_counter = 0
        read_stdin_flag = False
        stdout_ready_flag = False

        if QFTASM_RAM_AS_STDIN_BUFFER:
            python_stdin = sys.stdin.read()
            for i_str, c in enumerate(python_stdin):
                ram[QFTASM_RAMSTDIN_BUF_STARTPOSITION - i_str][0] = ord(c)
                ram[QFTASM_RAMSTDIN_BUF_STARTPOSITION - i_str][1] += 1
        elif stdin_from_pipe:
            python_stdin = sys.stdin.read()
        else:
            python_stdin = ""

    n_steps = 0

    prev_result_write_flag = False
    prev_result_value = 0
    prev_result_dst = 0

    # Main loop
    while ram[0][0] < len(rom):
        # 1. Fetch instruction
        pc = ram[0][0]
        inst = rom[pc]
        if type(inst) == str:
            rom[pc] = parser.parse_string(inst)[0]
            inst = rom[pc]
        opcode, (mode_1, d1), (mode_2, d2), (mode_3, d3) = inst

        # 2. Write the result of the previous instruction to the RAM
        if prev_result_write_flag:
            ram[prev_result_dst][0] = prev_result_value
            ram[prev_result_dst][1] += 1

        # 3. Read the data for the current instruction from the RAM
        for _ in range(mode_1):
            d1 = ram[d1][0]
        for _ in range(mode_2):
            d2 = ram[d2][0]
        for _ in range(mode_3):
            d3 = ram[d3][0]

        # 4. Compute the result
        prev_result_write_flag, prev_result_value, prev_result_dst = d_inst[opcode](d1, d2, d3)

        # 5. Increment the program counter
        ram[0][0] += 1
        ram[0][1] += 1


        # 6 (Extended): Manage the values of stdin and stdout
        if use_stdio:
            # stdin
            if not QFTASM_RAM_AS_STDIN_BUFFER:
                stdin = ram[QFTASM_STDIN][0]
                if pc > 1 and stdin == QFTASM_STDIO_OPEN:
                    if not stdin_from_pipe and stdin_counter >= len(python_stdin):
                        python_stdin += sys.stdin.readline()

                    if stdin_counter < len(python_stdin):
                        c = python_stdin[stdin_counter]
                        c_int = ord(c)
                        ram[QFTASM_STDIN][0] = c_int
                        stdin_counter += 1
                    else:
                        ram[QFTASM_STDIN][0] = 0

            if not QFTASM_RAM_AS_STDOUT_BUFFER:
                stdout = ram[QFTASM_STDOUT][0]
                # stdout
                if pc > 1 and stdout == QFTASM_STDIO_CLOSED:
                    stdout_ready_flag = True

                if pc > 1 and stdout != QFTASM_STDIO_CLOSED and stdout_ready_flag:
                    stdout_wrapped = stdout & ((1 << 8) - 1)
                    stdout_char = chr(stdout_wrapped)
                    print(stdout_char, end="")

                    stdout_ready_flag = False

        n_steps += 1

    if QFTASM_RAM_AS_STDOUT_BUFFER:
        stdout_buf = "".join([chr(ram[i_stdout][0] & ((1 << 8) - 1)) for i_stdout in reversed(range(ram[2][0]+1,QFTASM_RAMSTDOUT_BUF_STARTPOSITION+1))])
        print("".join(stdout_buf), end="")


    if debug_ramdump:
        n_nonzero_write_count_ram = 0
        n_nonzero_write_count_ram_maxindex = -1
        for i_index, (v, times) in enumerate(ram):
            if times > 0:
                n_nonzero_write_count_ram += 1
                n_nonzero_write_count_ram_maxindex = max(n_nonzero_write_count_ram_maxindex, i_index)
        print()
        print("ROM size: {}".format(len(rom_lines)))
        print("n_steps: {}".format(n_steps))
        print("Nonzero write count ram addresses: {}".format(n_nonzero_write_count_ram))
        print("Nonzero write count ram max address: {}".format(n_nonzero_write_count_ram_maxindex))

        print(ram[:20])

        # Requires numpy and matplotlib
        if debug_plot_memdist:
            import numpy as np
            import matplotlib.pyplot as plt
            a = np.array(ram[:n_nonzero_write_count_ram_maxindex+1])
            plt.figure()
            plt.plot(np.log(a[:,1]+1), "o-")
            plt.show()
            plt.savefig("./memdist.png")


if __name__ == "__main__":
    filepath = sys.argv[1]
    interpret_file(filepath)
