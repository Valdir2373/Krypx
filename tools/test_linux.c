


static void do_write(const char *buf, int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "0"(4), "b"(1), "c"(buf), "d"(len)
        : "memory"
    );
    (void)ret;
}


static void do_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        :: "a"(1), "b"(code)
        : "memory"
    );
    
    for (;;) __asm__ volatile ("hlt");
}


#define WRITE(s)  do_write((s), sizeof(s) - 1)


void _start(void) {
    WRITE("\n");
    WRITE("====================================================\n");
    WRITE("   Linux ELF i386 rodando dentro do Krypx OS!      \n");
    WRITE("====================================================\n");
    WRITE("\n");
    WRITE("[OK] Deteccao binaria  -> Linux ELF i386\n");
    WRITE("[OK] ELF Loader        -> segmentos PT_LOAD mapeados\n");
    WRITE("[OK] Ambiente Linux    -> processo isolado criado\n");
    WRITE("[OK] Syscall write(4)  -> VGA exibindo este texto\n");
    WRITE("[OK] Syscall exit(1)   -> processo terminando...\n");
    WRITE("\n");
    WRITE("  Sem libc. Sem CRT. So int $0x80 puro.\n");
    WRITE("  O Krypx traduz syscalls Linux para seu kernel.\n");
    WRITE("\n");
    do_exit(0);
}
