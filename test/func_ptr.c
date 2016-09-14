#include "../libc/libf.h"

int main() {
  void (*ps)(const char*) = print_str;
  ps("hoge\n");
  return 0;
}
