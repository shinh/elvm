#ifndef ELVM_LIBC_MATH_H_
#define ELVM_LIBC_MATH_H_

#define INFINITY 42
#define NAN 42

double ceil(double x) { return x; }
double floor(double x) { return x; }
double fmod(double x, double y) { return x % y; }
int isfinite(double x) { return 1; }
int isinf(double x) { return 0; }
int isnan(double x) { return 0; }
double pow(double x, double y) {
  double r = 1;
  for (double i = 0; i < y; i++)
    r *= x;
  return r;
}

#endif  // ELVM_LIBC_MATH_H_
