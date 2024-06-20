#!/usr/bin/python3

import re
import sys

def main(codeFile=None):
    if len(sys.argv) > 1:
        codeFile = sys.argv[1]
    elif codeFile is None:
        codeFile = input("Please supply a filename: ")
    try:
        with open(codeFile) as f:
            code = f.readlines()
        code = translate(code)
        exec(code, {"inputStream": inputStream()})
    except:
        print("Acc!!\n", file=sys.stderr)
        raise

def inputStream():
    buffer = ""
    while True:
        if buffer == "":
            try:
                buffer = input() + "\n"
            except EOFError:
                buffer = None
        if buffer is None:
            yield 0
        else:
            yield ord(buffer[0])
            buffer = buffer[1:]

def translate(accCode):
    indent = 0
    loopVars = []
    pyCode = ["_ = 0"]
    for lineNum, line in enumerate(accCode):
        if "#" in line:
            # Strip comments
            line = line[:line.index("#")]
        line = line.strip()
        if not line:
            continue
        lineNum += 1
        if line == "}":
            if indent:
                loopVar = loopVars.pop()
                pyCode.append(" "*indent + loopVar + " += 1")
                indent -= 1
                pyCode.append(" "*indent + "del " + loopVar)
            else:
                raise SyntaxError("Line %d: unmatched }" % lineNum)
        else:
            m = re.fullmatch(r"Count ([a-z]) while (.+) \{", line)
            if m:
                expression = validateExpression(m.group(2))
                if expression:
                    loopVar = m.group(1)
                    pyCode.append(" "*indent + loopVar + " = 0")
                    pyCode.append(" "*indent + "while " + expression + ":")
                    indent += 1
                    loopVars.append(loopVar)
                else:
                    raise SyntaxError("Line %d: invalid expression " % lineNum
                                      + m.group(2))
            else:
                m = re.fullmatch(r"Write (.+)", line)
                if m:
                    expression = validateExpression(m.group(1))
                    if expression:
                        pyCode.append(" "*indent
                                      + "print(chr(%s), end='')" % expression)
                    else:
                        raise SyntaxError("Line %d: invalid expression "
                                          % lineNum
                                          + m.group(1))
                else:
                    expression = validateExpression(line)
                    if expression:
                        pyCode.append(" "*indent + "_ = " + expression)
                    else:
                        raise SyntaxError("Line %d: invalid statement "
                                          % lineNum
                                          + line)
    return "\n".join(pyCode)

def validateExpression(expr):
    "Translates expr to Python expression or returns None if invalid."
    expr = expr.strip()
    if re.search(r"[^ 0-9a-z_N()*/%^+-]", expr):
        # Expression contains invalid characters
        return None
    elif re.search(r"[a-zN_]\w+", expr):
        # Expression contains multiple letters or underscores in a row
        return None
    else:
        # Not going to check validity of all identifiers or nesting of parens--
        # let the Python code throw an error if problems arise there
        # Replace short operators with their Python versions
        expr = expr.replace("^", "**")
        expr = expr.replace("/", "//")
        # Replace N with a call to get the next input character
        expr = expr.replace("N", "next(inputStream)")
        return expr

if __name__ == "__main__":
    main()
