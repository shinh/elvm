#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

void target_arm(Module* module);
void target_asmjs(Module* module);
void target_bef(Module* module);
void target_bf(Module* module);
void target_c(Module* module);
void target_cl(Module* module);
void target_cpp(Module* module);
void target_cpp_template(Module* module);
void target_cr(Module* module);
void target_cs(Module* module);
void target_el(Module* module);
void target_forth(Module* module);
void target_fs(Module* module);
void target_go(Module* module);
void target_hs(Module* module);
void target_i(Module* module);
void target_java(Module* module);
void target_js(Module* module);
void target_lua(Module* module);
void target_ll(Module* module);
void target_oct(Module* module);
void target_php(Module* module);
void target_piet(Module* module);
void target_pietasm(Module* module);
void target_pl(Module* module);
void target_py(Module* module);
void target_ps(Module* module);
void target_rb(Module* module);
void target_rs(Module* module);
void target_scala(Module* module);
void target_scm_sr(Module* module);
void target_sed(Module* module);
void target_sh(Module* module);
void target_sqlite3(Module* module);
void target_swift(Module* module);
void target_tex(Module* module);
void target_tf(Module* module);
void target_tm(Module* module);
void target_unl(Module* module);
void target_vim(Module* module);
void target_wasm(Module* module);
void target_ws(Module* module);
void target_x86(Module* module);

typedef void (*target_func_t)(Module*);

static target_func_t get_target_func(const char* ext) {
  if (!strcmp(ext, "arm")) return target_arm;
  if (!strcmp(ext, "asmjs")) return target_asmjs;
  if (!strcmp(ext, "bef")) return target_bef;
  if (!strcmp(ext, "bf")) {
    split_basic_block_by_mem();
    return target_bf;
  }
  if (!strcmp(ext, "c")) return target_c;
  if (!strcmp(ext, "cl")) return target_cl;
  if (!strcmp(ext, "cpp")) return target_cpp;
  if (!strcmp(ext, "cpp_template")) return target_cpp_template;
  if (!strcmp(ext, "cr")) return target_cr;
  if (!strcmp(ext, "cs")) return target_cs;
  if (!strcmp(ext, "el")) return target_el;
  if (!strcmp(ext, "forth")) return target_forth;
  if (!strcmp(ext, "fs")) return target_fs;
  if (!strcmp(ext, "go")) return target_go;
  if (!strcmp(ext, "hs")) return target_hs;
  if (!strcmp(ext, "i")) return target_i;
  if (!strcmp(ext, "java")) return target_java;
  if (!strcmp(ext, "js")) return target_js;
  if (!strcmp(ext, "lua")) return target_lua;
  if (!strcmp(ext, "ll")) return target_ll;
  if (!strcmp(ext, "oct")) return target_oct;
  if (!strcmp(ext, "php")) return target_php;
  if (!strcmp(ext, "piet")) return target_piet;
  if (!strcmp(ext, "pietasm")) return target_pietasm;
  if (!strcmp(ext, "pl")) return target_pl;
  if (!strcmp(ext, "py")) return target_py;
  if (!strcmp(ext, "ps")) return target_ps;
  if (!strcmp(ext, "rb")) return target_rb;
  if (!strcmp(ext, "rs")) return target_rs;
  if (!strcmp(ext, "scala")) return target_scala;
  if (!strcmp(ext, "scm_sr")) return target_scm_sr;
  if (!strcmp(ext, "sed")) return target_sed;
  if (!strcmp(ext, "sh")) return target_sh;
  if (!strcmp(ext, "sqlite3")) return target_sqlite3;
  if (!strcmp(ext, "swift")) return target_swift;
  if (!strcmp(ext, "tex")) return target_tex;
  if (!strcmp(ext, "tf")) return target_tf;
  if (!strcmp(ext, "tm")) return target_tm;
  if (!strcmp(ext, "unl")) return target_unl;
  if (!strcmp(ext, "vim")) return target_vim;
  if (!strcmp(ext, "wasm")) return target_wasm;
  if (!strcmp(ext, "ws")) return target_ws;
  if (!strcmp(ext, "x86")) return target_x86;
  error("unknown flag: %s", ext);
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
