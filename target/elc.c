#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

void target_rb(Module* module);
void target_py(Module* module);
void target_js(Module* module);
void target_x86(Module* module);
void target_ws(Module* module);

int main(int argc, char* argv[]) {
  void (*target_func)(Module*) = NULL;
  const char* filename = NULL;
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (arg[0] == '-') {
      if (!strcmp(arg, "-rb")) {
        target_func = target_rb;
      } else if (!strcmp(arg, "-py")) {
        target_func = target_py;
      } else if (!strcmp(arg, "-js")) {
        target_func = target_js;
      } else if (!strcmp(arg, "-x86")) {
        target_func = target_x86;
      } else if (!strcmp(arg, "-ws")) {
        target_func = target_ws;
      } else {
        error("unknown flag: %s", arg);
      }
    } else {
      filename = arg;
    }
  }

  if (!filename) {
    error("no input file");
  }
  if (!target_func) {
    error("no target");
  }

  Module* module = load_eir_from_file(filename);
  target_func(module);
}
