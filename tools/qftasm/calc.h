extern int getchar();
extern void putchar(int c);
extern int* _edata;
#define malloc(n) (_edata += n);

#define QFTASM_MEM_OFFSET 95
#define QFTASM_NATIVE_ADDR(x) (x - QFTASM_MEM_OFFSET)

#define STDIN_BUF_POINTER_REG ((char*) QFTASM_NATIVE_ADDR(1))
#define curchar() (*((char*) QFTASM_NATIVE_ADDR(*STDIN_BUF_POINTER_REG)))

int mul (int x, int y) {
    int ret = 0;
    while(x--) {
        ret += y;
    }
    return ret;
}
