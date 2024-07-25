#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define WORDMAX ((1 << 24))
// #define TRACE

typedef int64_t subleq_word;

typedef struct {
  char* type;
  char* value;
} MagicComment;

subleq_word dump_points[10000] = {0};

int run_subleq_bytes(subleq_word *code, subleq_word length){
  subleq_word pc = 0;

  while (pc < length - 2){
    subleq_word a = code[pc];
    subleq_word b = code[pc + 1];
    subleq_word c = code[pc + 2];

    if (a == -1){
      code[b] = getchar();
    } else if (b == -1){
      putchar((char)(code[a] & 255));
    } else if (c <= -1 && code[b] - code[a] <= 0){
      return 0;
    } else {

      code[b] -= code[a];

      if (code[b] <= 0){
        pc = c - 3;
      }
    }

    pc += 3;

  }

  return 0;

}

void skip_whitespace(FILE* fp){
  char c = getc(fp);
  while (c == ' ' || c == '\n' || c == '\r' || c == '\t'){
    c = getc(fp);
  }
  ungetc(c, fp);
}

void skip_to_newline(FILE* fp){
  while (getc(fp) != '\n' && !feof(fp)) continue;
}

MagicComment* parse_magic_comment(FILE* fp){
  getc(fp); // Discard first '{'

  MagicComment* mc = (MagicComment*)malloc(sizeof(MagicComment));
  if (!mc){
    fprintf(stderr, "Could not allocate magic comment\n");
  }

  char* buf = (char*)calloc(43, sizeof(char));
  if (!fgets(buf, 42, fp)) return 0;
  buf[strcspn(buf, "\n")] = 0;
  buf[strcspn(buf, "}")] = 0;

  mc->type = strtok(buf, ":");
  mc->value = strtok(NULL, ":");

  return mc;
}

int assemble_run_subleq(FILE* fp){
  #ifdef TRACE
  fprintf(stderr, "Converting to int32[]...\n");
  #endif
  
  subleq_word *code = (subleq_word*)calloc(32, sizeof(subleq_word));
  subleq_word current_size = 30;
  subleq_word loc = 0;

  char c = ' ';
  while (c != EOF){

    skip_whitespace(fp);

    c = getc(fp);
    if (c == EOF){
      break;
    }

    int dp_len = 0;

    switch (c){
      case '#':
        c = getc(fp);
        ungetc(c, fp);
        if (c == '{'){
          MagicComment* mc = parse_magic_comment(fp);
          if (!mc){
            break;
          }

          if (strcmp("loc_skip", mc->type) == 0) {
            int amnt = atoi(mc->value);
            loc += amnt - 1;
            fseek(fp, amnt * 2 - 1, SEEK_CUR);
          } else if (strcmp("dump_regs", mc->type) == 0) {
            dump_points[dp_len++] = loc;
          } else {
            fprintf(stderr, "Invalid magic comment {%s:%s}\n", mc->type, mc->value);
            return 1;
          }
          free(mc);
        } else {
          skip_to_newline(fp);
        }
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '-':
        ungetc(c, fp);
        if (fscanf(fp, "%ld", &code[loc++]) != 1) goto err;
        break;
      default:
      err:
        fprintf(stderr, "Invalid character %c (char code %d) at pos %ld", c, c, ftell(fp));
        return 1;
    }

    while (loc >= current_size){
      code = (subleq_word*)realloc(code, (current_size * 2) * sizeof(subleq_word));
      current_size *= 2;
      if (code == NULL){
        fprintf(stderr, "Couldn't allocate enough memory");
        return 1;
      }
    }

  }
    
  #ifdef TRACE
  fprintf(stderr, "Done.\n");
  #endif

  int ret = run_subleq_bytes(code, loc);
  free(code);
  return ret;

}

int main(int argc, char* argv[]){
  FILE* fp;

  if (argc == 2){
    fp = fopen(argv[1], "r");
  } else {
    fprintf(stderr, "Too many arguments");
    return 1;
  }

  if (fp == NULL){
    fprintf(stderr, "Failed to load subleq file");
    return 1;
  }

  int error_code = assemble_run_subleq(fp);

  fclose(fp);
  
  return error_code;
}