// A Funge-98 interpretter only with Befunge-93 operations.

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stack>
#include <string>
#include <vector>

using namespace std;

vector<vector<int> > code;
int ix, iy;
int vx, vy;
vector<int> st;
volatile bool signaled;

bool debug = false;
bool verbose = false;
bool bounce_on_fail_input = false;

inline void step() {
  if (vx) {
    ix += vx;
    if (ix < 0)
      ix = (int)code[iy].size() - 1;
    else if (ix >= (int)code[iy].size())
      ix = 0;
    return;
  }

restep:
  iy += vy;
  if (iy < 0) {
    iy = (int)code.size() - 1;
  }
  else if (iy >= (int)code.size()) {
    iy = 0;
  } else if (ix >= (int)code[iy].size()) {
    goto restep;
  }
}

inline int get() {
  assert(iy < (int)code.size());
  if (ix < (int)code[iy].size())
    return code[iy][ix];
  else
    return 0;
}

inline void push(int v) {
  st.push_back(v);
}

inline int pop() {
  if (st.empty())
    return 0;
  else {
    int r = st.back();
    st.pop_back();
    return r;
  }
}

void handleSignal(int) {
  signaled = true;
}

static void dump() {
  //fprintf(stderr, "%sc", "\x1b");
  fprintf(stderr, "=== MEMORY ===\n");
  for (int y = 0; y < (int)code.size(); y++) {
    for (int x = 0; x < (int)code[y].size(); x++) {
      char c = code[y][x];
      bool color = true;
      if (x == ix && y == iy) {
        fprintf(stderr, "\x1b[41m");
      } else if (c < 32) {
        fprintf(stderr, "\x1b[30m\x1b[47m");
        c = c < 10 ? '0' + c : 'A' + c - 10;
      } else {
        color = false;
      }
      fputc(c, stderr);
      if (color) {
        fprintf(stderr, "\x1b[39m");
        fprintf(stderr, "\x1b[49m");
      }
    }
    fputc('\n', stderr);
  }

  fprintf(stderr, "=== STACK ===\n");
  for (int v : st) {
    fprintf(stderr, "%d ", v);
  }
  fprintf(stderr, "\n");

  fprintf(stderr, "\n");
}

void handleUnsupportedOp(char op) {
  dump();
  fprintf(stderr,
          "this implementation doesn't support '%c' (%d) @%d,%d\n",
          op, op, ix, iy);
  exit(1);
}

int main(int argc, char* argv[]) {
  srand(time(NULL));

  const char* prog = argv[0];

  while (argc >= 2 && argv[1][0] == '-') {
    if (!strcmp(argv[1], "-g")) {
      debug = true;
    } else if (!strcmp(argv[1], "-v")) {
      verbose = true;
    } else {
      fprintf(stderr, "unknown switch %s\n", argv[1]);
      return 1;
    }
    argc--;
    argv++;
  }

  if (argc < 2) {
    fprintf(stderr, "%s <src.bef>\n", prog);
    exit(1);
  }

  FILE* fp = fopen(argv[1], "rb");
  if (!fp) {
    fprintf(stderr, "failed to open: %s\n", argv[1]);
    exit(1);
  }
  char* buf = NULL;
  size_t buf_len;
  ssize_t len;
  while ((len = getline(&buf, &buf_len, fp)) >= 0) {
    if (buf[len-1] == '\n') {
      buf[len-1] = '\0';
    }
    code.push_back(vector<int>());
    for (char* p = buf; *p; p++) {
      code.back().push_back(*p);
    }
  }
  fclose(fp);

#if 0
  signal(SIGINT, &handleSignal);
  signal(SIGSEGV, &handleSignal);
#endif

  ix = iy = 0;
  vx = 1;
  vy = 0;
  for (;;) {
    int op = code[iy][ix];
    //fprintf(stderr, "op=%c\n", op);
    switch (op) {
    case '<':
      vx = -1;
      vy = 0;
      break;
    case '>':
      vx = 1;
      vy = 0;
      break;
    case '^':
      vx = 0;
      vy = -1;
      break;
    case 'v':
      vx = 0;
      vy = 1;
      break;

    case '_':
      vy = 0;
      if (pop()) {
        vx = -1;
      } else {
        vx = 1;
      }
      break;
    case '|':
      vx = 0;
      if (pop()) {
        vy = -1;
      } else {
        vy = 1;
      }
      break;

    case '?': 
      switch (rand() / (RAND_MAX / 4)) {
      case 0:
        vx = 1;
        vy = 0;
        break;
      case 1:
        vx = -1;
        vy = 0;
        break;
      case 2:
        vx = 0;
        vy = 1;
        break;
      case 3:
        vx = 0;
        vy = -1;
        break;
      }
      break;

    case ' ':
      break;

    case '#':
      step();
      if (debug && code[iy][ix] == 'S') {
        dump();
      } else if (verbose && code[iy][ix] == 'V') {
        dump();
      }
      break;

    case '@': {
      if (debug) {
        fprintf(stderr, "program finished\n");
        dump();
      }
      exit(0);
    }

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      push(op - '0');
      break;

    case '"':
      step();
      for (;;) {
        int c = get();
        if (c == '"')
          break;
        push(c);
        step();
      }
      break;

    case '&': {
      int v;
      if (scanf("%d", &v) == 1) {
        push(v);
      } else {
        if (bounce_on_fail_input) {
          vx = -vx;
          vy = -vy;
        } else {
          push(-1);
        }
      }
      break;
    }
    case '~': {
      int v = getchar();
      if (v != EOF || !bounce_on_fail_input) {
        push(v);
      } else {
        vx = -vx;
        vy = -vy;
      }
      break;
    }

    case ',':
      putchar(pop());
      break;
    case '.':
      printf("*%d*\n", pop());
      break;

    case '+': {
      int y = pop();
      push(pop() + y);
      break;
    }
    case '-': {
      int y = pop();
      push(pop() - y);
      break;
    }
    case '*': {
      int y = pop();
      push(pop() * y);
      break;
    }
    case '/': {
      int y = pop();
      push(pop() / y);
      break;
    }
    case '%': {
      int y = pop();
      push(pop() % y);
      break;
    }
    case '`': {
      int y = pop();
      push(pop() > y);
      break;
    }
    case '!':
      push(!pop());
      break;

    case ':': {
      int v = pop();
      push(v);
      push(v);
      break;
    }
    case '\\': {
      int y = pop();
      int x = pop();
      push(y);
      push(x);
      break;
    }
    case '$':
      pop();
      break;

    case 'g': {
      int y = pop();
      int x = pop();
      int v = 0;
      if (y >= 0 && x >= 0 && y < (int)code.size() && x < (int)code[y].size())
        v = code[y][x];
      //fprintf(stderr, "g x=%d y=%d v=%d\n", x, y, v);
      push(v);
      break;
    }
    case 'p': {
      //dump();
      int y = pop();
      int x = pop();
      int v = pop();
      //fprintf(stderr, "p x=%d y=%d v=%d\n", x, y, v);
      if (y < 0 || x < 0) {
        dump();
        fprintf(stderr, "negative 'p' isn't supported x=%d y=%d\n", x, y);
        exit(1);
      }
      if (y >= (int)code.size()) {
        code.resize(y + 1);
      }
      if (x >= (int)code[y].size()) {
        code[y].resize(x + 1);
      }
      code[y][x] = v;
      break;
    }

#if 0
    case 'S':
      if (!debug)
        goto unsupported;
      dump();
      break;
#endif

    default:
      // We are very strict for unsupported operations.
      handleUnsupportedOp(op);

    }
    step();

    if (signaled) {
      if (debug) {
        dump();
      }
      exit(1);
    }
  }
}
