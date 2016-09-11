#include "ir.h"

#include <stdlib.h>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "no input file\n");
    exit(1);
  }
  Module* m = load_eir_from_file(argv[1]);
  for (Inst* inst = m->text; inst; inst = inst->next) {
    dump_inst(inst);
  }
  return 0;
}
