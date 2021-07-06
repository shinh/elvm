#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef int32_t subleq_word;

int run_subleq_bytes(subleq_word *code, size_t length){
  size_t pc = 0;

  while (pc < length){
    subleq_word a = code[pc];
    subleq_word b = code[pc + 1];
    subleq_word c = code[pc + 2];
    
    if (a < -1 || a >= (subleq_word)length){
      printf("%d %d %d\n", a, b, c);
      fprintf(stderr, "Value of a = %d is outside of range %zu (at loc %zu)", a, length, pc);
      return 1;
    } else if (b < -1 || b >= (subleq_word)length){
      printf("%d %d %d\n", a, b, c);
      fprintf(stderr, "Value of b = %d is outside of range %zu (at loc %zu)", b, length, pc);
      return 1;
    } else if (c >= (subleq_word)length){
      printf("%d %d %d\n", a, b, c);
      fprintf(stderr, "Value of c = %d is outside of range %zu (at loc %zu)", c, length, pc);
      return 1;
    }

    printf("loc: %zu, [%d] = %d, [%d] = %d, goto %d\n", pc, a, code[a], b, code[b], c);

    if (a == -1){
      code[b] = getchar();
    } else if (b == -1){
      putchar(code[a] & 255);
    } 
    
    if (c <= -1 && code[b] - code[a] <= 0){
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

int assemble_run_subleq(FILE* fp){
  subleq_word *code = (subleq_word*)calloc(30, sizeof(subleq_word));
  size_t current_size = 30;
  size_t loc = 0;

  char c = getc(fp);
  while (c != EOF){
    ungetc(c, fp);

    subleq_word i = 0;
    fscanf(fp, "%d", &i);
    code[loc++] = i;

    if (loc >= current_size){
      code = (subleq_word*)realloc(code, current_size * 2 * sizeof(subleq_word));
      current_size *= 2;
    }

    c = getc(fp);
  }

  code = (subleq_word*)realloc(code, loc * sizeof(subleq_word));
  return run_subleq_bytes(code, loc);

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
  
  if (error_code != 0){
    exit(error_code);
  }
}