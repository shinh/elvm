#ifndef ELVM_LIBC_BUILTIN_H_
#define ELVM_LIBC_BUILTIN_H_

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

static int __builtin_mul(int a, int b) {
  int i, e, v;
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

static unsigned int __builtin_div(unsigned int a, unsigned int b) {
  _my_div_t r;
  my_div(a, b, &r);
  return r.quot;
}

static unsigned int __builtin_mod(unsigned int a, unsigned int b) {
  _my_div_t r;
  my_div(a, b, &r);
  return r.rem;
}

static void __builtin_to_bitarray(unsigned int v, unsigned char* b) {
  static const int bits_table[] = {
    0x800000, 0x400000, 0x200000, 0x100000,
    0x80000, 0x40000, 0x20000, 0x10000,
    0x8000, 0x4000, 0x2000, 0x1000,
    0x800, 0x400, 0x200, 0x100,
    0x80, 0x40, 0x20, 0x10,
    0x8, 0x4, 0x2, 0x1,
  };
  for (int i = 0; i < 24; i++) {
    int t = bits_table[i];
    if (v >= t) {
      b[i] = 1;
      v -= t;
    } else {
      b[i] = 0;
    }
  }
}

static unsigned int __builtin_from_bitarray(unsigned char* b) {
  unsigned int v = 0;
  for (int i = 0; i < 24; i++) {
    v += v;
    v += b[i];
  }
  return v;
}

static unsigned int __builtin_and(unsigned int a, unsigned int b) {
  unsigned char av[24];
  unsigned char bv[24];
  __builtin_to_bitarray(a, av);
  __builtin_to_bitarray(b, bv);
  for (int i = 0; i < 24; i++) {
    av[i] = av[i] && bv[i];
  }
  return __builtin_from_bitarray(av);
}

static unsigned int __builtin_or(unsigned int a, unsigned int b) {
  unsigned char av[24];
  unsigned char bv[24];
  __builtin_to_bitarray(a, av);
  __builtin_to_bitarray(b, bv);
  for (int i = 0; i < 24; i++) {
    av[i] = av[i] || bv[i];
  }
  return __builtin_from_bitarray(av);
}

static unsigned int __builtin_xor(unsigned int a, unsigned int b) {
  unsigned char av[24];
  unsigned char bv[24];
  __builtin_to_bitarray(a, av);
  __builtin_to_bitarray(b, bv);
  for (int i = 0; i < 24; i++) {
    av[i] = av[i] != bv[i];
  }
  return __builtin_from_bitarray(av);
}

static unsigned int __builtin_not(unsigned int a) {
  unsigned char av[24];
  __builtin_to_bitarray(a, av);
  for (int i = 0; i < 24; i++) {
    av[i] = !av[i];
  }
  return __builtin_from_bitarray(av);
}

static unsigned int __builtin_shl(unsigned int a, unsigned int b) {
  unsigned char av[48];
  __builtin_to_bitarray(a, av);
  for (int i = 0; i < b; i++) {
    av[24+i] = 0;
  }
  return __builtin_from_bitarray(av + b);
}

static unsigned int __builtin_shr(unsigned int a, unsigned int b) {
  unsigned char av[48];
  __builtin_to_bitarray(a, av + 24);
  for (int i = 0; i < b; i++) {
    av[24-i-1] = 0;
  }
  return __builtin_from_bitarray(av + 24 - b);
}

#endif  // ELVM_LIBC_BUILTIN_H_
