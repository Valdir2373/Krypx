

#include "elf.h"
#include "dynlink.h"
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/string.h>
#include <drivers/vga.h>
#include <fs/vfs.h>
#include <types.h>

/* User stack for 32-bit Linux ELF processes */
#define USER_STACK_PAGE      0xBFFFC000ULL
#define USER_STACK_PAGES     4U
#define USER_STACK_SIZE      (USER_STACK_PAGES * 0x1000U)
#define USER_STACK_TOP_INIT  (USER_STACK_PAGE + USER_STACK_SIZE - 0x100U)

bool elf_validate(const uint8_t *data, size_t size) {
    if (!data || size < sizeof(elf32_hdr_t)) return false;

    const elf32_hdr_t *h = (const elf32_hdr_t *)data;

    if (h->e_ident[0] != 0x7F || h->e_ident[1] != 'E' ||
        h->e_ident[2] != 'L'  || h->e_ident[3] != 'F')
        return false;

    if (h->e_ident[EI_DATA] != ELFDATA2LSB) return false;

    /* Accept ELF32 i386 or ELF64 x86_64 */
    if (h->e_ident[EI_CLASS] == ELFCLASS32) {
        if (h->e_machine != EM_386) return false;
    } else if (h->e_ident[EI_CLASS] == ELFCLASS64) {
        const elf64_hdr_t *h64 = (const elf64_hdr_t *)data;
        if (h64->e_machine != EM_X86_64) return false;
    } else {
        return false;
    }

    if (h->e_type != ET_EXEC && h->e_type != ET_DYN) return false;
    if (h->e_phnum == 0 || h->e_phoff == 0) return false;

    return true;
}

static int load_pt_load32(process_t *proc, const uint8_t *data,
                           const elf32_phdr_t *ph) {
    if (ph->p_memsz == 0) return 0;

    uint64_t virt_start = (uint64_t)ph->p_vaddr & ~0xFFFULL;
    uint64_t virt_end   = ((uint64_t)ph->p_vaddr + ph->p_memsz + 0xFFFULL) & ~0xFFFULL;
    uint64_t virt;

    for (virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return -1;
        uint64_t flags = PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE;
        vmm_map_page(proc->page_dir, virt, phys, flags);
    }

    memset((void *)(uintptr_t)virt_start, 0, (size_t)(virt_end - virt_start));
    if (ph->p_filesz > 0)
        memcpy((void *)(uintptr_t)ph->p_vaddr, data + ph->p_offset, ph->p_filesz);

    return 0;
}

static int load_pt_load64(process_t *proc, const uint8_t *data,
                           const elf64_phdr_t *ph) {
    if (ph->p_memsz == 0) return 0;

    uint64_t virt_start = ph->p_vaddr & ~0xFFFULL;
    uint64_t virt_end   = (ph->p_vaddr + ph->p_memsz + 0xFFFULL) & ~0xFFFULL;
    uint64_t virt;

    for (virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return -1;
        uint64_t flags = PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE;
        vmm_map_page(proc->page_dir, virt, phys, flags);
    }

    memset((void *)(uintptr_t)virt_start, 0, (size_t)(virt_end - virt_start));
    if (ph->p_filesz > 0)
        memcpy((void *)(uintptr_t)ph->p_vaddr, data + ph->p_offset, (size_t)ph->p_filesz);

    return 0;
}

int elf_load(process_t *proc, const uint8_t *data, size_t size,
             elf_load_result_t *result) {
    if (!elf_validate(data, size)) return -1;

    const elf32_hdr_t *hdr32 = (const elf32_hdr_t *)data;
    int is64 = (hdr32->e_ident[EI_CLASS] == ELFCLASS64);

    result->is_linux_compat = (hdr32->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                                hdr32->e_ident[EI_OSABI] == ELFOSABI_LINUX);
    result->is_dynamic = false;

    pml4e_t *saved_dir = vmm_get_current_dir();
    vmm_switch_address_space(proc->page_dir);

    uint64_t segment_end = 0;
    uint16_t i, phnum;

    if (!is64) {
        phnum = hdr32->e_phnum;
        /* Check for PT_INTERP (dynamic) */
        for (i = 0; i < phnum; i++) {
            const elf32_phdr_t *ph = (const elf32_phdr_t *)
                (data + hdr32->e_phoff + (uint32_t)i * hdr32->e_phentsize);
            if (ph->p_type == PT_INTERP) result->is_dynamic = true;
        }
        /* Load PT_LOAD segments */
        for (i = 0; i < phnum; i++) {
            const elf32_phdr_t *ph = (const elf32_phdr_t *)
                (data + hdr32->e_phoff + (uint32_t)i * hdr32->e_phentsize);
            if (ph->p_type != PT_LOAD) continue;
            if (load_pt_load32(proc, data, ph) != 0) {
                vmm_switch_address_space(saved_dir);
                return -1;
            }
            uint64_t seg_end = (uint64_t)ph->p_vaddr + ph->p_memsz;
            if (seg_end > segment_end) segment_end = seg_end;
        }

        /* Map user stack */
        for (i = 0; i < USER_STACK_PAGES; i++) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) { vmm_switch_address_space(saved_dir); return -1; }
            vmm_map_page(proc->page_dir,
                         USER_STACK_PAGE + (uint64_t)i * PAGE_SIZE, phys,
                         PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }
        memset((void *)(uintptr_t)USER_STACK_PAGE, 0, USER_STACK_SIZE);

        /* Initial stack frame: argc / argv[0] / NULL / envp NULL / AT_NULL */
        uint64_t name_virt = USER_STACK_PAGE + USER_STACK_SIZE - 0x20U;
        size_t nlen = strlen(proc->name);
        if (nlen > 15) nlen = 15;
        memcpy((void *)(uintptr_t)name_virt, proc->name, nlen);
        ((char *)(uintptr_t)name_virt)[nlen] = '\0';

        uint32_t *sp = (uint32_t *)(uintptr_t)USER_STACK_TOP_INIT;
        sp[0] = 1;                    /* argc */
        sp[1] = (uint32_t)name_virt;  /* argv[0] */
        sp[2] = 0;                    /* argv[1] = NULL */
        sp[3] = 0;                    /* envp[0] = NULL */
        sp[4] = 0;                    /* AT_NULL */
        sp[5] = 0;

        vmm_switch_address_space(saved_dir);
        result->entry_point    = (uint64_t)hdr32->e_entry;
        result->user_stack_top = USER_STACK_TOP_INIT;
        result->heap_base      = (segment_end + 0xFFFULL) & ~0xFFFULL;
    } else {
        /* ELF64 */
        const elf64_hdr_t *hdr64 = (const elf64_hdr_t *)data;
        phnum = hdr64->e_phnum;

        /* Collect PT_INTERP path and check for dynamic */
        char interp_path[256];
        interp_path[0] = 0;
        uint64_t phdr_vaddr = 0;
        for (i = 0; i < phnum; i++) {
            const elf64_phdr_t *ph = (const elf64_phdr_t *)
                (data + hdr64->e_phoff + (uint64_t)i * hdr64->e_phentsize);
            if (ph->p_type == PT_INTERP) {
                result->is_dynamic = true;
                uint32_t plen = (ph->p_filesz < 255) ? (uint32_t)ph->p_filesz : 255;
                memcpy(interp_path, data + ph->p_offset, plen);
                interp_path[plen] = 0;
            }
        }

        /* For PIE (ET_DYN) executables, choose a load base */
        uint64_t load_base = 0;
        if (hdr64->e_type == ET_DYN && !result->is_dynamic)
            load_base = 0x400000ULL;  /* non-interp PIE */
        else if (hdr64->e_type == ET_DYN)
            load_base = 0x400000ULL;  /* Firefox / PIE with interp */

        /* Load PT_LOAD segments */
        for (i = 0; i < phnum; i++) {
            const elf64_phdr_t *ph = (const elf64_phdr_t *)
                (data + hdr64->e_phoff + (uint64_t)i * hdr64->e_phentsize);
            if (ph->p_type == PT_PHDR) {
                phdr_vaddr = load_base + ph->p_vaddr;
            }
            if (ph->p_type != PT_LOAD) continue;
            /* For PIE, shift by load_base */
            elf64_phdr_t shifted = *ph;
            shifted.p_vaddr += load_base;
            if (load_pt_load64(proc, data, &shifted) != 0) {
                vmm_switch_address_space(saved_dir);
                return -1;
            }
            uint64_t seg_end = load_base + ph->p_vaddr + ph->p_memsz;
            if (seg_end > segment_end) segment_end = seg_end;
        }

        /* Map user stack — 32 pages below canonical top */
        uint64_t stack_pages = 32;
        uint64_t stack_base  = 0x7FFFFFFC0000ULL - stack_pages * PAGE_SIZE;
        for (i = 0; i < (uint16_t)stack_pages; i++) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) { vmm_switch_address_space(saved_dir); return -1; }
            vmm_map_page(proc->page_dir,
                         stack_base + (uint64_t)i * PAGE_SIZE, phys,
                         PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }
        uint64_t stack_top = stack_base + stack_pages * PAGE_SIZE - 0x100ULL;
        memset((void *)(uintptr_t)stack_base, 0, (size_t)(stack_pages * PAGE_SIZE));

        uint64_t prog_entry = load_base + hdr64->e_entry;

        if (result->is_dynamic && interp_path[0]) {
            /* Load interpreter and set up auxv-based stack */
            dynlink_result_t dl;
            if (dynlink_load(proc, interp_path, prog_entry,
                              phdr_vaddr ? phdr_vaddr : (load_base + hdr64->e_phoff),
                              hdr64->e_phentsize, hdr64->e_phnum,
                              stack_top, proc->name, &dl) == 0) {
                vmm_switch_address_space(saved_dir);
                result->entry_point    = dl.interp_entry;
                result->user_stack_top = dl.stack_pointer;
                result->heap_base      = (segment_end + 0x200000ULL) & ~0x1FFFFFULL;
                return 0;
            }
            /* dynlink failed — fall through to static execution attempt */
        }

        vmm_switch_address_space(saved_dir);
        result->entry_point    = prog_entry;
        result->user_stack_top = stack_top;
        result->heap_base      = (segment_end + 0xFFFULL) & ~0xFFFULL;
    }

    return 0;
}
