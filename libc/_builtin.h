#ifndef ELVM_LIBC_BUILTIN_H_
#define ELVM_LIBC_BUILTIN_H_

#ifdef __clang__
#define float int
#define double int
#endif

typedef struct {
  int quot, rem;
} _my_div_t;

// Our 8cc doesn't support returning a structure value.
// TODO: update 8cc.
void my_div(unsigned int a, unsigned int b, _my_div_t* o) {
  unsigned int d[24];
  unsigned int r[24];
  unsigned int i;
  r[0] = 1;
  for (i = 0;; i++) {
    d[i] = b;
    unsigned int nb = b + b;
    if (nb > a || nb < b)
      break;
    r[i+1] = r[i] + r[i];
    b = nb;
  }

  unsigned int q = 0;
  for (;; i--) {
    unsigned int v = d[i];
    if (a >= v) {
      q += r[i];
      a -= v;
    }
    if (i == 0)
      break;
  }
  o->quot = q;
  o->rem = a;
}

int __elvm_builtin_mul(int a, int b) {
  int i, e, v;
  if (a < b) {
    v = a;
    a = b;
    b = v;
  }
  if (b == 1)
    return a;
  if (b == 0)
    return 0;
  int d[24];
  int r[24];
  for (i = 0, e = 1, v = a;; i++) {
    d[i] = v;
    r[i] = e;
    v += v;
    int ne = e + e;
    if (ne < e || ne > b)
      break;
    e = ne;
  }

  int x = 0;
  for (;; i--) {
    if (b >= r[i]) {
      x += d[i];
      b -= r[i];
    }
    if (i == 0)
      break;
  }
  return x;
}

unsigned int __elvm_builtin_div(unsigned int a, unsigned int b) {
  if (b == 1)
    return a;
  _my_div_t r;
  my_div(a, b, &r);
  return r.quot;
}

unsigned int __elvm_builtin_mod(unsigned int a, unsigned int b) {
  _my_div_t r;
  my_div(a, b, &r);
  return r.rem;
}

const int __elvm_builtin_bits_table[] = {
  0x800000, 0x400000, 0x200000, 0x100000,
  0x80000, 0x40000, 0x20000, 0x10000,
  0x8000, 0x4000, 0x2000, 0x1000,
  0x800, 0x400, 0x200, 0x100,
  0x80, 0x40, 0x20, 0x10,
  0x8, 0x4, 0x2, 0x1,
};

#define __ELVM_BUILTIN_TO_BIT(v, t) (v >= t ? (v -= t, 1) : 0)

unsigned int __elvm_builtin_and(unsigned int a, unsigned int b) {
  int r = 0;
  for (int i = 0; i < 24; i++) {
    int t = __elvm_builtin_bits_table[i];
    int a1 = __ELVM_BUILTIN_TO_BIT(a, t);
    int b1 = __ELVM_BUILTIN_TO_BIT(b, t);
    if (a1 && b1)
      r += t;
  }
  return r;
}

unsigned int __elvm_builtin_or(unsigned int a, unsigned int b) {
  int r = 0;
  for (int i = 0; i < 24; i++) {
    int t = __elvm_builtin_bits_table[i];
    int a1 = __ELVM_BUILTIN_TO_BIT(a, t);
    int b1 = __ELVM_BUILTIN_TO_BIT(b, t);
    if (a1 || b1)
      r += t;
  }
  return r;
}

unsigned int __elvm_builtin_xor(unsigned int a, unsigned int b) {
  int r = 0;
  for (int i = 0; i < 24; i++) {
    int t = __elvm_builtin_bits_table[i];
    int a1 = __ELVM_BUILTIN_TO_BIT(a, t);
    int b1 = __ELVM_BUILTIN_TO_BIT(b, t);
    if (a1 != b1)
      r += t;
  }
  return r;
}

unsigned int __elvm_builtin_not(unsigned int a) {
  int r = 0;
  for (int i = 0; i < 24; i++) {
    int t = __elvm_builtin_bits_table[i];
    int a1 = __ELVM_BUILTIN_TO_BIT(a, t);
    if (!a1)
      r += t;
  }
  return r;
}

unsigned int __elvm_builtin_shl(unsigned int a, unsigned int b) {
  int r = 0;
  for (int i = b; i < 24; i++) {
    int t = __elvm_builtin_bits_table[i];
    int a1 = __ELVM_BUILTIN_TO_BIT(a, t);
    if (a1)
      r += __elvm_builtin_bits_table[i-b];
  }
  return r;
}

unsigned int __elvm_builtin_shr(unsigned int a, unsigned int b) {
  int r = 0;
  for (int i = b; i < 24; i++) {
    int t = __elvm_builtin_bits_table[i-b];
    int a1 = __ELVM_BUILTIN_TO_BIT(a, t);
    if (a1)
      r += __elvm_builtin_bits_table[i];
  }
  return r;
}

#endif  // ELVM_LIBC_BUILTIN_H_
