#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

void target_rb(Module* module);
void target_py(Module* module);
void target_js(Module* module);
void target_el(Module* module);
void target_vim(Module* module);
void target_tex(Module* module);
void target_sh(Module* module);
void target_java(Module* module);
void target_c(Module* module);
void target_x86(Module* module);
void target_i(Module* module);
void target_ws(Module* module);
void target_piet(Module* module);
void target_pietasm(Module* module);
void target_bef(Module* module);
void target_bf(Module* module);
void target_unl(Module* module);
typedef void (*target_func_t)(Module*);

static target_func_t get_target_func(const char* ext) {
  if (!strcmp(ext, "rb")) {
    return target_rb;
  } else if (!strcmp(ext, "py")) {
    return target_py;
  } else if (!strcmp(ext, "js")) {
    return target_js;
  } else if (!strcmp(ext, "el")) {
    return target_el;
  } else if (!strcmp(ext, "vim")) {
    return target_vim;
  } else if (!strcmp(ext, "tex")) {
    return target_tex;
  } else if (!strcmp(ext, "sh")) {
    return target_sh;
  } else if (!strcmp(ext, "java")) {
    return target_java;
  } else if (!strcmp(ext, "c")) {
    return target_c;
  } else if (!strcmp(ext, "x86")) {
    return target_x86;
  } else if (!strcmp(ext, "i")) {
    return target_i;
  } else if (!strcmp(ext, "ws")) {
    return target_ws;
  } else if (!strcmp(ext, "piet")) {
    return target_piet;
  } else if (!strcmp(ext, "pietasm")) {
    return target_pietasm;
  } else if (!strcmp(ext, "bef")) {
    return target_bef;
  } else if (!strcmp(ext, "bf")) {
    split_basic_block_by_mem();
    return target_bf;
  } else if (!strcmp(ext, "unl")) {
    return target_unl;
  } else {
    error("unknown flag: %s", ext);
  }
}

int main(int argc, char* argv[]) {
#if defined(NOFILE) || defined(__eir__)
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
