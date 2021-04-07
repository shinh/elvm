import sys
import re

filepath = sys.argv[1]

with open(filepath, "rt") as f:
    text = f.read()

d = {}
for i_line, line in enumerate(text.split("\n")):
    m = re.search(r'pc == ([0-9]+|EXIT|TABLE):', line)
    if m:
        if m.group(1) == "EXIT":
            d["EXIT"] = i_line - 1
        elif m.group(1) == "TABLE":
            d["TABLE"] = i_line
        else:
            d["pc" + m.group(1)] = i_line

text = text.format(**d)
text = text[:-1] # Remove the newline at the end

print(text, end="")
