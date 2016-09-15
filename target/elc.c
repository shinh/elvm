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
typedef void (*target_func_t)(Module*);

static target_func_t get_target_func(const char* ext) {
  if (!strcmp(ext, "rb")) {
    return target_rb;
  } else if (!strcmp(ext, "py")) {
    return target_py;
  } else if (!strcmp(ext, "js")) {
    return target_js;
  } else if (!strcmp(ext, "x86")) {
    return target_x86;
  } else if (!strcmp(ext, "ws")) {
    return target_ws;
  } else {
    error("unknown flag: %s", ext);
  }
}

int main(int argc, char* argv[]) {
#ifdef NOFILE
  char buf[32];
  for (int i = 0;; i++) {
    int c = getchar();
    if (c == '\n' || c == EOF) {
      buf[i] = 0;
      break;
    }
    buf[i] = c;
  }
  target_func_t target_func = get_target_func(buf);
  Module* module = load_eir(stdin);
#else
  target_func_t target_func = NULL;
  const char* filename = NULL;
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (arg[0] == '-') {
      target_func = get_target_func(arg + 1);
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
#endif
  target_func(module);
}
