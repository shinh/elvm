#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>



/**
 * Converts a decimal ascii code into a character which will then be printed to
 * stdout.
 *
 * Required by CMake backend since CMake does not handle binary data well.
 */
int main(int argc, char const** argv) {
  if (2 != argc) {
    fprintf(stderr, "Usage: putc <decimal-ascii-code>\n");
    return EXIT_FAILURE;
  }

  char const* decimal_ascii_code = argv[1];
  int const ascii_code = atoi(decimal_ascii_code);

  if ((ascii_code < 0) || (ascii_code > UINT8_MAX)) {
    fprintf(stderr, "Invalid ascii code %d\n", ascii_code);
    return EXIT_FAILURE;
  }

  fprintf(stdout, "%c", ascii_code);
  return EXIT_SUCCESS;
}

