/* linker.ld — x86_64 kernel Krypx
 * Kernel carregado pelo GRUB em 0x00100000 (1 MB), executa em 64-bit long mode.
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)

SECTIONS {
    . = 0x00100000;

    /* Seção .multiboot — DEVE vir primeiro (GRUB procura nos primeiros 8 KB) */
    .multiboot ALIGN(4) : {
        *(.multiboot)
    }

    /* Código executável */
    .text ALIGN(4096) : {
        *(.text)
        *(.text.*)
    }

    /* Dados somente-leitura (strings constantes, tabelas) */
    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Dados inicializados */
    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
    }

    /* Dados não-inicializados (zero na carga) */
    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Símbolo que marca o fim do kernel — usado pelo PMM para saber
     * onde o kernel termina e a memória livre começa */
    . = ALIGN(4096);
    kernel_end = .;
}
