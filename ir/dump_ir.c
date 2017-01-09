#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>

int main(int argc, char* argv[]) {
#if defined(NOFILE) || defined(__eir__)
  Module* m = load_eir(stdin);
  // Host dump_ir.c.exe should dump to stdout for testing.
  stderr = stdout;
#else
  while (1) {
    if (argc >= 2 && !strcmp(argv[1], "-f")) {
      handle_call_ret();
      handle_logic_ops();
      argc--;
      argv++;
      continue;
    }
    break;
  }
  if (argc < 2) {
    fprintf(stderr, "no input file\n");
    exit(1);
  }
  Module* m = load_eir_from_file(argv[1]);
#endif
  for (Inst* inst = m->text; inst; inst = inst->next) {
    dump_inst(inst);
  }
  return 0;
}
