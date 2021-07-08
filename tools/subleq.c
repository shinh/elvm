#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//#define TRACE
typedef int32_t subleq_word;

int run_subleq_bytes(subleq_word *code, subleq_word length){
  subleq_word pc = 0;

  while (pc < length - 2){
    subleq_word a = code[pc];
    subleq_word b = code[pc + 1];
    subleq_word c = code[pc + 2];
    
    if (a < -1 || a >= length){
      printf("%d %d %d\n", a, b, c);
      fprintf(stderr, "Value of a = %d is outside of range %d (at loc %d)\n", a, length, pc);
      return 1;
    } else if (b < -1 || b >= length){
      printf("%d %d %d\n", a, b, c);
      fprintf(stderr, "Value of b = %d is outside of range %d (at loc %d)\n", b, length, pc);
      return 1;
    } else if (c >= length - 2){
      printf("%d %d %d (%d)\n", a, b, c, length - c);
      fprintf(stderr, "Value of c = %d is outside of range %d (at loc %d)\n", c, length, pc);
      return 1;
    }

    #ifdef TRACE
    printf("loc: %d (%d), [%d] = %d, [%d] = %d, goto %d\n", pc, code[pc + 3], a, code[a], b, code[b], c);
    printf("A: %d  B: %d  C:%d  D:%d  SP:%d  BP:%d  PC:%d\n",
              code[3], code[4], code[5], code[6], code[7], code[8], code[9]);
    #endif

    if (a == -1){
      code[b] = (subleq_word)getchar();
      if (code[b] == EOF){
        code[b] = 0;
      }
    } else if (b == -1){
      putchar(code[a] & 255);
    } else if (c <= -1 && code[b] - code[a] <= 0){
      return 0;
    } else {

      code[b] -= code[a];

      if (code[b] <= 0){
        pc = c - 3;
      }
    }

    #ifdef TRACE
    printf("After:   loc: %d (%d), [%d] = %d, [%d] = %d, goto %d\n", pc, code[pc + 3], a, code[a], b, code[b], c);
    printf("After:   A: %d  B: %d  C:%d  D:%d  SP:%d  BP:%d  PC:%d\n",
              code[3], code[4], code[5], code[6], code[7], code[8], code[9]);
    #endif

    pc += 3;

  }

  return 0;

}

int assemble_run_subleq(FILE* fp){
  subleq_word *code = (subleq_word*)calloc(30, sizeof(subleq_word));
  subleq_word current_size = 30;
  subleq_word loc = 0;

  char c = getc(fp);
  while (c != EOF){

    while (c == ' ' || c == '\n' || c == '\r' || c == '\t'){
      c = getc(fp);
    }

    if (c == EOF){
      break;
    }

    if (('0' > c || c > '9') && c != '-' && c != '#' 
          && c != ' ' && c != '\n' && c != '\r' && c != '\t'){
      fprintf(stderr, "Invalid character %c (%d)", c, c);
      return 1;
    } else if (c == '#'){
      c = getc(fp);
      while(c != '\n' && c != EOF){
        c = getc(fp);
      }

      c = getc(fp);
      continue;
    }

    ungetc(c, fp);

    subleq_word i = 0;
    fscanf(fp, "%d", &i);
    code[loc++] = i;

    if (loc >= current_size){
      code = (subleq_word*)realloc(code, current_size * 2 * sizeof(subleq_word));
      current_size *= 2;
      if (code == NULL){
        fprintf(stderr, "Couldn't allocate enough memory");
        return 1;
      }
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
  
  return error_code;
}