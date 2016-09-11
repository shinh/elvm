#include <stdio.h>
#include <stdlib.h>

#include <ir/ir.h>

void target_rb(Module* module);

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "no input file\n");
    return 1;
  }

  Module* module = load_eir_from_file(argv[1]);
  target_rb(module);
}
