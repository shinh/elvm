#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#ifdef __GNUC__
#if __has_attribute(fallthrough)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif
#else
#define FALLTHROUGH
#endif

typedef unsigned char byte;

using namespace std;

bool g_trace;
bool g_verbose;

struct Loop;

enum OpType {
  OP_MEM,
  OP_PTR,
  OP_LOOP,
  OP_COMMENT,
};

struct Op {
  char op;
  int arg;
  Loop* loop;
  string comment;

  Op()
      : op(0), arg(0) {
  }
};

struct Loop {
  vector<Op*> code;
  map<int, int> addsub;
  int ptr;
  bool has_io;

  Loop() {
    reset(NULL);
  }

  void reset(vector<Op*>* out) {
    if (out)
      copy(code.begin(), code.end(), back_inserter(*out));

    code.clear();
    addsub.clear();
    ptr = 0;
    has_io = false;
  }
};

int merge_ops(char add, char sub, const char** code) {
  int r = 0;
  const char* p;
  for (p = *code; *p; p++) {
    if (*p == add)
      r++;
    else if (*p == sub)
      r--;
    else
      break;
  }
  *code = p - 1;
  return r;
}

void parse(const char* code, vector<Op*>* ops) {
  Loop* cur_loop = new Loop();
  vector<int> loop_stack;
  for (const char* p = code; *p; p++) {
    char c = *p;
    Op* op = new Op();
    switch (c) {
      case '+':
      case '-': {
        op->arg = merge_ops('+', '-', &p);
        if (op->arg) {
          op->op = OP_MEM;
          cur_loop->addsub[cur_loop->ptr] += op->arg;
        } else {
          delete op;
          op = NULL;
        }
        break;
      }

      case '>':
      case '<': {
        op->arg = merge_ops('>', '<', &p);
        if (op->arg) {
          op->op = OP_PTR;
          cur_loop->ptr += op->arg;
        } else {
          delete op;
          op = NULL;
        }
        break;
      }

      case '.':
      case ',': {
        op->op = c;
        cur_loop->has_io = true;
        break;
      }

      case '[': {
        cur_loop->reset(ops);
        op->op = c;
        loop_stack.push_back(ops->size());
        break;
      }

      case ']':
        if (loop_stack.empty()) {
          fprintf(stderr, "unmatched close paren\n");
          exit(1);
        }

        if (!cur_loop->has_io && cur_loop->ptr == 0 &&
            cur_loop->addsub[0] == -1) {
          op->op = OP_LOOP;
          op->loop = cur_loop;
          cur_loop = new Loop();
          cur_loop->has_io = true;
        } else {
          cur_loop->reset(ops);
          op->op = c;
          op->arg = loop_stack.back();
          (*ops)[op->arg]->arg = ops->size();
        }
        loop_stack.pop_back();
        break;

      case '#':
        if (g_trace && p[1] == '{') {
          op->op = OP_COMMENT;
          for (p += 2; *p != '}'; p++) {
            op->comment += *p;
          }
        } else {
          goto nop;
        }
        break;

      case '@':
        if (g_verbose) {
          op->op = c;
          break;
        }
        goto nop;

      default:
      nop:
        delete op;
        op = NULL;
    }

    if (op)
      cur_loop->code.push_back(op);
  }

  cur_loop->reset(ops);

  if (!loop_stack.empty()) {
    fprintf(stderr, "unmatched open paren\n");
    exit(1);
  }
}

void check_bound(int mp) {
  if (mp < 0) {
    fprintf(stderr, "memory pointer out of bound\n");
    exit(1);
  }
}

void alloc_mem(size_t mp, vector<byte>* mem) {
  if (mp >= mem->size()) {
    mem->resize(mp * 2);
  }
}

int read_mem(const vector<byte>& mem, int index) {
  return mem[index-1] * 65536 + mem[index] * 256 + mem[index+1];
}

void dump_state(const vector<byte>& mem) {
  static const char* kRegs[] = {
    "PC", "A", "B", "C", "D", "BP", "SP"
  };
  for (int i = 0; i < 7; i++) {
    if (i)
      printf(" ");
    int v = read_mem(mem, 8 + 6 * i);
    if (i == 0)
      v--;
    printf("%s=%d", kRegs[i], v);
  }
  printf("\n");
  fflush(stdout);
}

void run(const vector<Op*>& ops) {
  int mp = 0;
  vector<byte> mem(1);
  for (size_t pc = 0; pc < ops.size(); pc++) {
    const Op* op = ops[pc];
    switch (op->op) {
      case '+':
        mem[mp]++;
        break;

      case '-':
        mem[mp]--;
        break;

      case OP_MEM:
        mem[mp] += op->arg;
        break;

      case '>':
        mp++;
        alloc_mem(mp, &mem);
        break;

      case '<':
        mp--;
        check_bound(mp);
        break;

      case OP_PTR:
        mp += op->arg;
        check_bound(mp);
        alloc_mem(mp, &mem);
        break;

      case '.':
        putchar(mem[mp]);
        break;

      case ',':
        mem[mp] = getchar();
        break;

      case '[':
        if (mem[mp] == 0)
          pc = op->arg;
        break;

      case ']':
        pc = op->arg - 1;
        break;

      case OP_LOOP: {
        int v = mem[mp];
        mem[mp] = 0;
        for (map<int, int>::const_iterator iter = op->loop->addsub.begin();
             iter != op->loop->addsub.end();
             ++iter) {
          int p = iter->first;
          int d = iter->second;
          if (p != 0) {
            alloc_mem(mp + p, &mem);
            mem[mp + p] += v * d;
          }
        }
        break;
      }

      case OP_COMMENT: {
        fprintf(stderr, "TRACE %s %f %zu\n",
                op->comment.c_str(),
                static_cast<double>(clock()) / CLOCKS_PER_SEC,
                pc);
        break;
      }

      case '@':
        if (g_verbose)
          dump_state(mem);
        break;

    }
  }
}

void compile(const vector<Op*>& ops, const char* fname) {
  FILE* fp = fopen(fname, "wb");
  fprintf(fp, "#include <stdio.h>\n");
  fprintf(fp, "unsigned char mem[4096*4096*10];\n");
  fprintf(fp, "int main() {\n");
  fprintf(fp, "unsigned char* mp = mem;\n");

  for (size_t pc = 0; pc < ops.size(); pc++) {
    const Op* op = ops[pc];
    switch (op->op) {
      case '+':
        fprintf(fp, "++*mp;\n");
        break;

      case '-':
        fprintf(fp, "--*mp;\n");
        break;

      case OP_MEM:
        if (op->arg)
          fprintf(fp, "*mp += %d;\n", op->arg);
        break;

      case '>':
        fprintf(fp, "mp++;\n");
        break;

      case '<':
        fprintf(fp, "mp--;\n");
        break;

      case OP_PTR:
        if (op->arg)
          fprintf(fp, "mp += %d;\n", op->arg);
        break;

      case '.':
        fprintf(fp, "putchar(*mp);\n");
        break;

      case ',':
        fprintf(fp, "*mp = getchar();\n");
        break;

      case '[':
        fprintf(fp, "while (*mp) {\n");
        break;

      case ']':
        fprintf(fp, "}\n");
        break;

      case OP_LOOP: {
        for (map<int, int>::const_iterator iter = op->loop->addsub.begin();
             iter != op->loop->addsub.end();
             ++iter) {
          int p = iter->first;
          int d = iter->second;
          if (p != 0) {
            fprintf(fp, "mp[%d] += *mp * %d;\n", p, d);
          }
        }
        fprintf(fp, "*mp = 0;\n");
        break;
      }

    }
  }

  fprintf(fp, "return 0;\n");
  fprintf(fp, "}\n");
  fclose(fp);
}

int main(int argc, char* argv[]) {
  bool should_compile = false;
  const char* arg0 = argv[0];
  while (argc >= 2 && argv[1][0] == '-') {
    if (!strcmp(argv[1], "-c")) {
      should_compile = true;
    } else if (!strcmp(argv[1], "-t")) {
      g_trace = true;
    } else if (!strcmp(argv[1], "-v")) {
      g_verbose = true;
    } else {
      fprintf(stderr, "Unknown flag: %s\n", argv[1]);
      return 1;
    }
    argc--;
    argv++;
  }

  if (argc < 2 || (argc < 3 && should_compile)) {
    fprintf(stderr, "Usage: %s <bf>\n", arg0);
    return 1;
  }

  const char* fname = argv[1];
  FILE* fp = fopen(fname, "rb");
  if (!fp) {
    perror("open");
    return 1;
  }
  string buf;
  while (true) {
    int c = fgetc(fp);
    if (c == EOF)
      break;
    if (c == '#') {
      c = fgetc(fp);
      if (c == '{') {
        buf += "#{";
        for (; c != '}';) {
          c = fgetc(fp);
          buf += c;
        }
      }
    }
    if (strchr("+-<>.,[]@", c))
      buf += c;
  }
  fclose(fp);

  vector<Op*> ops;
  parse(buf.c_str(), &ops);
  if (should_compile)
    compile(ops, argv[2]);
  else
    run(ops);
}
