/* Simulator for deterministic Turing machines.

   Usage: tm [-b] [-n] [-v|-vv] <tm-file> 

   where <tm-file> contains a description of a Turing machine. Each
   line in the machine description specifies a transition:

   q a r b d

   where 
   - q is the current state
   - a is the symbol read from the tape
   - r is the new state
   - b is the symbol written to the tape
   - d is a direction: L (left), N (no move), or R (right)

   A state can be any integer. The start state is 0. The accept state
   is -1.

   A symbol can be any non-whitespace character. The blank symbol is
   _.

   The input string is read from stdin:

   - If the -b switch is given, the input bytes are encoded on the
     tape in binary, most significant bit first. For example, "ABC"
     would be encoded as 010000010100001001000011___.... Machines
     compiled from EIR should be run with this switch.

   - If the -n switch is given, nothing is read from stdin, and the
     input string is empty.

   The tape extends infinitely to the right, but not to the left. The
   head starts on the first square of the tape. If the machine
   attempts to move the head to the left of the first square, the head
   remains on the first square.

   If the machine enters the accept state, the contents of the tape
   are written to stdout, and the simulator exits with status 0. If
   the -b switch is given, the tape is decoded using the same encoding
   as described above. Symbols that are neither 0 nor 1 are ignored,
   as are incomplete bytes.

   If, at any time, a move is not possible, the simulator exits with
   status 1.

   When compiled with NOFILE or __eir__ defined, the simulator takes
   no command-line arguments and reads everything from stdin: the
   machine description, a separator line "--", and the input string.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(NOFILE) || defined(__eir__)
#else
#include <unistd.h>
#endif

int verbose = 0;
bool binary_mode = false;
bool read_input = true;

void error(const char *s) {
  fputs(s, stderr);
  exit(1);
}

void usage() {
  error("usage: tm [-n] [-b] [-v|-vv] <tm-file>\n"
        "\n"
        "-n   don't read input string\n"
        "-b   encode input and output strings in base 2\n"
        "-v   show progress occasionally\n"
        "-vv  show each step\n"
        );
}
  
typedef int state_t;
typedef char symbol_t;
const symbol_t BLANK = '_';
const symbol_t ONE = '1';
const symbol_t ZERO = '0';

typedef struct {
  state_t q;  // from state
  symbol_t a; // read symbol
  state_t r;  // to state
  symbol_t b; // write symbol
  int d;      // move (-1, 0, or 1)
} transition;

int compare_transitions(transition *t1, transition *t2) {
  if (t1->q > t2->q) return 1;
  if (t1->q == t2->q) {
    if (t1->a > t2->a) return 1;
    else if (t1->a == t2->a) return 0;
  }
  return -1;
}

int tok_int(char *line, char **lasts) {
  char *field = strtok_r(line, " ", lasts);
  if (field != NULL) {
    char *endptr;
    int i = strtol(field, &endptr, 10);
    if (*endptr == '\0') 
      return i;
  }
  error("expected an integer\n");
  return 0;
}

char tok_char(char *line, char **lasts) {
  char *field = strtok_r(line, " ", lasts);
  if (field != NULL)
    if (strlen(field) == 1)
      return field[0];
  error("expected a single character\n");
  return 0;
}

typedef struct {
  transition *transitions;
  size_t capacity, size;
  size_t *index, index_size;
} dtm;

dtm *new_dtm() {
  dtm *m = malloc(sizeof(dtm));
  m->capacity = 1024;
  m->transitions = malloc(m->capacity * sizeof(transition));
  m->size = 0;
  return m;
}

void append_transition(dtm *m, state_t q, symbol_t a, state_t r, symbol_t b, int d) {
  if (m->size == m->capacity) {
    transition *old = m->transitions;
    m->capacity *= 2;
    m->transitions = malloc(m->capacity * sizeof(transition));
    memcpy(m->transitions, old, m->size * sizeof(transition));
    free(old);
  }
  m->transitions[m->size].q = q;
  m->transitions[m->size].a = a;
  m->transitions[m->size].r = r;
  m->transitions[m->size].b = b;
  m->transitions[m->size].d = d;
  m->size++;
}

void index_transitions(dtm *m) {
  qsort(m->transitions, m->size, sizeof(transition), (int (*)(const void *,const void *))compare_transitions);
  if (m->size == 0) return;
  state_t qmax = m->transitions[m->size-1].q;
  m->index_size = qmax+2;
  m->index = malloc(m->index_size * sizeof(size_t));
  state_t q = 0;
  m->index[0] = 0;
  for (size_t i=0; i<m->size; i++) {
    while (m->transitions[i].q > q) {
      q++;
      m->index[q] = i;
    }
  }
  m->index[qmax+1] = m->size;
}

bool find_transition(dtm *m, state_t q, symbol_t a, state_t *pr, symbol_t *pb, int *pd) {
  if (q == -1 || (size_t)q >= m->index_size) return false;
  for (size_t i=m->index[q]; i<m->index[q+1]; i++)
    if (m->transitions[i].a == a) {
      *pr = m->transitions[i].r;
      *pb = m->transitions[i].b;
      *pd = m->transitions[i].d;
      return true;
    }
  return false;
}

typedef struct {
  char *chars;
  size_t capacity, size;
} string;

string *new_string() {
  string *s = malloc(sizeof(string));
  s->capacity = 10;
  s->chars = malloc(s->capacity * sizeof(char));
  s->chars[0] = '\0';
  s->size = 0;
  return s;
}

void string_append(string *s, char c) {
  if (s->size+1 == s->capacity) {
    char *old = s->chars;
    s->capacity *= 2;
    s->chars = malloc(s->capacity * sizeof(char));
    memcpy(s->chars, old, (s->size+1) * sizeof(char));
    free(old);
  }
  s->chars[s->size] = c;
  s->size++;
  s->chars[s->size] = '\0';
}

void string_pop(string *s) {
  if (s->size > 0) {
    s->size--;
    s->chars[s->size] = '\0';
  }  
}

void string_clear(string *s) {
  s->chars[0] = '\0';
  s->size = 0;
}

int read_line(FILE *fp, string *s) {
  int c;
  string_clear(s);
  c = fgetc(fp);
  if (c == EOF)
    return 0;
  while (c != EOF && c != '\n') {
    string_append(s, c);
    c = fgetc(fp);
  }
  return 1;
}

#if defined(NOFILE) || defined(__eir__)
int main() {
  FILE *fp = stdin;
  verbose = 2;
#else
int main(int argc, char *argv[]) {
  int ch;
  while ((ch = getopt(argc, argv, "nbv")) != -1) {
    switch (ch) {
    case 'n':
      read_input = false;
      break;
    case 'b':
      binary_mode = true;
      break;
    case 'v': 
      verbose++;
      break;
    default: 
      usage();
    }
  }
  argc -= optind;
  argv += optind;
  if (argc != 1) usage();
  
  // Read TM
  char *filename = argv[0];
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) error("error opening file\n");
#endif
  dtm *m = new_dtm();
  string *line = new_string();
  while (read_line(fp, line)) {
    if (strncmp(line->chars, "//", 2) == 0)
      continue;
    if (strncmp(line->chars, "--", 2) == 0)
      break;
    char *lasts;
    state_t q = tok_int(line->chars, &lasts);
    symbol_t a = tok_char(NULL, &lasts);
    state_t r = tok_int(NULL, &lasts);
    symbol_t b = tok_char(NULL, &lasts);
    int d = 0;
    switch (tok_char(NULL, &lasts)) {
    case 'L': d = -1; break;
    case 'N': d = 0; break;
    case 'R': d = +1; break;
    default: error("direction must be L, N, or R\n");
    }
    if (strtok_r(NULL, " ", &lasts) != NULL)
      error("extra field in line\n");
    append_transition(m, q, a, r, b, d);
  }
  index_transitions(m);

  // Read and encode input string
  string *tape = new_string();
  if (read_input) {
    if (binary_mode) {
      int c;
      while ((c = fgetc(stdin)) != EOF)
        for (int j=7; j>=0; j--)
          string_append(tape, c & (1<<j) ? ONE : ZERO);
    } else {
      read_line(stdin, tape);
    }
  }

  // Initialize TM configuration
  state_t q = 0;
  size_t pos = 0;
  size_t max_pos = 0;      // to measure space
  long long int steps = 0; // to measure time
  bool accept;
  while (true) {
    while (pos+1 > tape->size)
      string_append(tape, BLANK);
    while (tape->chars[tape->size-1] == BLANK && tape->size > pos+1)
      string_pop(tape);
    if (verbose >= 2) {
      fprintf(stderr, "%d | ", q);
      for (size_t i=0; i<tape->size; i++) {
	if (i == pos)
          fprintf(stderr, "[%c]", tape->chars[i]);
	else
          fputc(tape->chars[i], stderr);
      }
      fputc('\n', stderr);
    }
    if (q == -1) {
      accept = true;
      break;
    }
    int d;
    if (!find_transition(m, q, tape->chars[pos], &q, &tape->chars[pos], &d)) {
      accept = false;
      break;
    }
    if (d == 1)
      ++pos;
    else if (d == -1 && pos > 0)
      --pos;
    ++steps;
    if (pos > max_pos)
      max_pos = pos;
    if (verbose >= 1 && steps % 10000000 == 0)
      fprintf(stderr, "running: steps=%lld cells=%lu\n", steps, max_pos+1);
  }
  if (verbose >= 1)
    fprintf(stderr, "halt: accept=%d steps=%lld cells=%lu\n", accept, steps, max_pos+1);

  if (accept) {
    if (binary_mode) {
      char c = 0;
      for (size_t i=0; i<tape->size; i++) {
        if (tape->chars[i] == ONE)
          c = (c<<1) | 1;
        else if (tape->chars[i] == ZERO)
          c = (c<<1) | 0;
        if (i % 8 == 7) {
          putchar(c);
          c = 0;
        }
      }
    } else {
      puts(tape->chars);
    }
    exit(0);
  } else {
    exit(1);
  }
}
