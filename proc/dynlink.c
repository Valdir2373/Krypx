/*
 * proc/dynlink.c — Kernel-side dynamic linker bootstrap for Krypx.
 *
 * When an ELF has PT_INTERP (e.g. Firefox linked against ld-musl-x86_64.so.1),
 * this code:
 *   1. Loads the interpreter from VFS at DYNLINK_INTERP_BASE
 *   2. Writes a proper Linux-ABI stack (argc/argv/envp/auxv)
 *   3. Returns the interpreter's entry point
 *
 * The interpreter (ld-musl / ld-linux) then loads all DT_NEEDED libraries via
 * mmap() syscalls and relocates the main binary before calling its entry point.
 */

#include "dynlink.h"
#include "elf.h"
#include <fs/vfs.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <lib/string.h>
#include <types.h>

/* Search paths for the interpreter */
static const char *interp_search[] = {
    "",       /* absolute path as-is */
    "/lib",
    "/usr/lib",
    "/usr/lib/x86_64-linux-musl",
    0
};

static vfs_node_t *find_file(const char *path) {
    if (path[0] == '/') return vfs_resolve(path);
    char buf[256];
    int i;
    for (i = 0; interp_search[i]; i++) {
        buf[0] = 0;
        if (interp_search[i][0]) {
            strncpy(buf, interp_search[i], 200);
            strncat(buf, "/", 255 - strlen(buf));
            strncat(buf, path, 255 - strlen(buf));
        } else {
            strncpy(buf, path, 255);
        }
        vfs_node_t *n = vfs_resolve(buf);
        if (n) return n;
    }
    return 0;
}

/* Load all PT_LOAD segments of an ELF64 file at (base + vaddr).
 * For a PIE/DSO (ET_DYN), base is applied; for ET_EXEC, base=0.
 * Returns the entry point. */
static int load_elf64_segments(process_t *proc, const uint8_t *data, size_t size,
                                uint64_t base, uint64_t *entry_out) {
    if (!data || size < sizeof(elf64_hdr_t)) return -1;
    const elf64_hdr_t *hdr = (const elf64_hdr_t *)data;
    if (hdr->e_ident[0] != 0x7F || hdr->e_ident[1] != 'E' ||
        hdr->e_ident[2] != 'L'  || hdr->e_ident[3] != 'F') return -1;
    if (hdr->e_ident[EI_CLASS] != ELFCLASS64) return -1;

    uint16_t i;
    for (i = 0; i < hdr->e_phnum; i++) {
        const elf64_phdr_t *ph = (const elf64_phdr_t *)
            (data + hdr->e_phoff + (uint64_t)i * hdr->e_phentsize);
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_memsz == 0) continue;

        uint64_t vstart = (base + ph->p_vaddr) & ~0xFFFULL;
        uint64_t vend   = (base + ph->p_vaddr + ph->p_memsz + 0xFFFULL) & ~0xFFFULL;
        uint64_t v;
        for (v = vstart; v < vend; v += PAGE_SIZE) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) return -1;
            vmm_map_page(proc->page_dir, v, phys,
                         PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }
        memset((void *)(uintptr_t)vstart, 0, (size_t)(vend - vstart));
        if (ph->p_filesz > 0)
            memcpy((void *)(uintptr_t)(base + ph->p_vaddr),
                   data + ph->p_offset, (size_t)ph->p_filesz);
    }

    *entry_out = base + hdr->e_entry;
    return 0;
}

/* Write a 64-bit word to user stack and advance pointer */
static inline void push64(uint64_t **sp, uint64_t val) {
    *sp -= 1;
    **sp = val;
}

int dynlink_load(process_t *proc,
                 const char *interp_path,
                 uint64_t prog_entry,
                 uint64_t phdr_vaddr,
                 uint16_t phentsize,
                 uint16_t phnum,
                 uint64_t stack_top,
                 const char *argv0,
                 dynlink_result_t *result) {

    /* Find and load the interpreter */
    vfs_node_t *node = find_file(interp_path);
    if (!node || node->size == 0) return -1;

    uint8_t *idata = (uint8_t *)kmalloc(node->size);
    if (!idata) return -1;
    vfs_read(node, 0, node->size, idata);

    uint64_t interp_entry = 0;
    int r = load_elf64_segments(proc, idata, node->size,
                                 DYNLINK_INTERP_BASE, &interp_entry);
    kfree(idata);
    if (r != 0) return -1;

    /* ── Build Linux-ABI initial stack ────────────────────────────────────── */
    /*
     * Layout (high → low, RSP ends at the bottom):
     *   [strings: argv0, platform string, at_random bytes]
     *   [padding to 16-byte align]
     *   [AT_NULL pair]
     *   [auxv pairs ...]
     *   [NULL  (end of envp)]
     *   [NULL  (end of argv)]
     *   [argv[0] pointer]
     *   [argc = 1]
     *   <- RSP
     */

    /* Place string data at top of stack area */
    uint64_t sp = stack_top & ~0xFULL;

    /* at_random: 16 bytes of pseudo-random data */
    sp -= 16;
    uint64_t at_random_ptr = sp;
    {
        uint8_t *r_bytes = (uint8_t *)(uintptr_t)sp;
        uint8_t seed = 0xAB;
        int k;
        for (k = 0; k < 16; k++) { seed ^= (seed << 3) | (seed >> 5); r_bytes[k] = seed; }
    }

    /* Platform string */
    const char *plat = "x86_64";
    sp -= (uint64_t)(strlen(plat) + 1);
    uint64_t plat_ptr = sp;
    memcpy((void *)(uintptr_t)sp, plat, strlen(plat) + 1);

    /* argv[0] string */
    const char *a0 = argv0 ? argv0 : "prog";
    uint64_t a0len = strlen(a0) + 1;
    sp -= a0len;
    uint64_t argv0_ptr = sp;
    memcpy((void *)(uintptr_t)sp, a0, (size_t)a0len);

    /* Align to 16 bytes */
    sp &= ~0xFULL;

    /* Build auxv, envp, argv, argc as an array and copy onto stack */
    /* We use a local buffer then copy */
    uint64_t auxv_buf[64];
    int ai = 0;
#define AUX(t,v) do { auxv_buf[ai++]=(t); auxv_buf[ai++]=(v); } while(0)
    AUX(AT_PHDR,   phdr_vaddr);
    AUX(AT_PHENT,  (uint64_t)phentsize);
    AUX(AT_PHNUM,  (uint64_t)phnum);
    AUX(AT_PAGESZ, 4096ULL);
    AUX(AT_BASE,   DYNLINK_INTERP_BASE);
    AUX(AT_FLAGS,  0ULL);
    AUX(AT_ENTRY,  prog_entry);
    AUX(AT_UID,    0ULL);
    AUX(AT_EUID,   0ULL);
    AUX(AT_GID,    0ULL);
    AUX(AT_EGID,   0ULL);
    AUX(AT_HWCAP,  0x1F8BFBFFULL);  /* typical x86_64 hwcap */
    AUX(AT_CLKTCK, 100ULL);
    AUX(AT_RANDOM, at_random_ptr);
    AUX(AT_SECURE, 0ULL);
    AUX(AT_EXECFN, argv0_ptr);
    AUX(AT_HWCAP2, 0ULL);
    AUX(AT_NULL,   0ULL);
#undef AUX

    /* Total words: argc(1) + argv[0](1) + argv_null(1) + envp_null(1) + auxv */
    int nwords = 1 + 1 + 1 + 1 + ai;
    /* Must be 16-byte aligned after RSP is set: (nwords * 8) % 16 == 8 or 0 */
    if (nwords % 2 == 0) nwords++;  /* make total odd so argc is at 16-byte aligned - 8 */

    sp -= (uint64_t)nwords * 8;
    sp &= ~0xFULL;
    /* Re-align: Linux ABI requires (rsp + 8) % 16 == 0 at entry */
    /* So (sp) % 16 == 8 after subtracting 8 for return address in main */
    if (sp % 16 == 0) sp -= 8;

    uint64_t *p64 = (uint64_t *)(uintptr_t)sp;
    int idx = 0;
    p64[idx++] = 1;            /* argc */
    p64[idx++] = argv0_ptr;    /* argv[0] */
    p64[idx++] = 0;            /* argv[NULL] */
    p64[idx++] = 0;            /* envp[NULL] */
    int k;
    for (k = 0; k < ai; k++) p64[idx++] = auxv_buf[k];

    result->interp_base  = DYNLINK_INTERP_BASE;
    result->interp_entry = interp_entry;
    result->stack_pointer = sp;
    return 0;
}
