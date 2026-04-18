; boot.asm — Krypx x86_64 entry
; GRUB loads us in 32-bit protected mode via Multiboot1.
; We set up initial page tables and enter 64-bit long mode,
; then call kernel_main(uint64_t magic, uint64_t mbi_addr).

bits 32

; ─── Multiboot1 header ────────────────────────────────────────────────────────
MBOOT_MAGIC    equ 0x1BADB002
MBOOT_FLAGS    equ 0x00000007   ; align modules + memory map + video info request
MBOOT_CHECKSUM equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM
    dd 0, 0, 0, 0, 0   ; load address fields (unused with flags bit 1 = 0 here)
    dd 0                ; graphics: 0 = linear framebuffer
    dd 1024             ; preferred width
    dd 768              ; preferred height
    dd 32               ; preferred bpp

; ─── BSS: page tables + kernel stack ─────────────────────────────────────────
section .bss
align 4096

; PML4 → PDPT → PD (using 2MB pages for identity map)
; We need enough tables to cover 0 – 4 GB (4 PDs × 512 × 2MB = 4 GB)
pml4_table:  resb 4096
pdpt_table:  resb 4096
pd_table0:   resb 4096   ; maps 0 –   1 GB
pd_table1:   resb 4096   ; maps 1 –   2 GB
pd_table2:   resb 4096   ; maps 2 –   3 GB
pd_table3:   resb 4096   ; maps 3 –   4 GB

; 64 KB kernel boot stack (16-byte aligned)
align 16
stack_bottom:
    resb 65536
stack_top:

; ─── Data: 64-bit GDT (minimal, for entering long mode) ──────────────────────
section .data
align 8
gdt64:
    dq 0x0000000000000000    ; 0x00 — null descriptor
    dq 0x00AF9A000000FFFF    ; 0x08 — kernel code64 (L=1, DPL=0)
    dq 0x00AF92000000FFFF    ; 0x10 — kernel data64  (DPL=0)
gdt64_end:

; NOTE: GDTR uses 2-byte limit + 4-byte base here (still in 32-bit mode).
; The C-side gdt_init() will reload a proper 10-byte GDTR in 64-bit mode.
gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dd gdt64

; ─── Entry point ─────────────────────────────────────────────────────────────
section .text
global _start
extern kernel_main

_start:
    ; Save multiboot args: will become rdi/rsi (SysV x86_64 calling convention)
    mov [mb_magic], eax
    mov [mb_info],  ebx

    ; ── Build initial page tables (identity map 0–4 GB with 2 MB PSE pages) ──

    ; Fill pd_table0: 512 entries × 2 MB = 1 GB, starting at 0
    mov edi, pd_table0
    xor eax, eax
.fill_pd0:
    mov ecx, eax
    shl ecx, 21             ; ecx = entry_index × 2MB
    or  ecx, 0x83           ; Present + Writable + PS (2MB page)
    mov [edi], ecx
    mov dword [edi+4], 0
    add edi, 8
    inc eax
    cmp eax, 512
    jne .fill_pd0

    ; Fill pd_table1: 512 × 2MB = 1GB, starting at 1GB
    mov edi, pd_table1
    mov eax, 512            ; start at entry 512 (address 512×2MB = 1GB)
.fill_pd1:
    mov ecx, eax
    shl ecx, 21
    or  ecx, 0x83
    mov [edi], ecx
    mov dword [edi+4], 0
    add edi, 8
    inc eax
    cmp eax, 1024
    jne .fill_pd1

    ; Fill pd_table2: 512 × 2MB = 1GB, starting at 2GB
    mov edi, pd_table2
    mov eax, 1024
.fill_pd2:
    mov ecx, eax
    shl ecx, 21
    or  ecx, 0x83
    mov [edi], ecx
    mov dword [edi+4], 0
    add edi, 8
    inc eax
    cmp eax, 1536
    jne .fill_pd2

    ; Fill pd_table3: 512 × 2MB = 1GB, starting at 3GB
    mov edi, pd_table3
    mov eax, 1536
.fill_pd3:
    mov ecx, eax
    shl ecx, 21
    or  ecx, 0x83
    mov [edi], ecx
    mov dword [edi+4], 0
    add edi, 8
    inc eax
    cmp eax, 2048
    jne .fill_pd3

    ; PDPT: entries 0–3 point to the four PDs
    mov eax, pd_table0
    or  eax, 0x03           ; Present + Writable
    mov [pdpt_table +  0], eax
    mov dword [pdpt_table +  4], 0

    mov eax, pd_table1
    or  eax, 0x03
    mov [pdpt_table +  8], eax
    mov dword [pdpt_table + 12], 0

    mov eax, pd_table2
    or  eax, 0x03
    mov [pdpt_table + 16], eax
    mov dword [pdpt_table + 20], 0

    mov eax, pd_table3
    or  eax, 0x03
    mov [pdpt_table + 24], eax
    mov dword [pdpt_table + 28], 0

    ; PML4: entry 0 → PDPT
    mov eax, pdpt_table
    or  eax, 0x03
    mov [pml4_table], eax
    mov dword [pml4_table + 4], 0

    ; ── Enter long mode ───────────────────────────────────────────────────────

    ; 1. Enable PAE (CR4.PAE = bit 5)
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    ; 2. Load PML4 into CR3
    mov eax, pml4_table
    mov cr3, eax

    ; 3. Set IA32_EFER.LME (bit 8) via MSR 0xC0000080
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    ; 4. Enable paging (CR0.PG = bit 31) — activates IA-32e (compatibility) mode
    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax
    ; We are now in IA-32e compatibility mode (32-bit code, 64-bit paging)

    ; 5. Load the 64-bit GDT
    lgdt [gdt64_ptr]

    ; 6. Far jump into 64-bit long mode (loads CS = 0x08 = kernel code64)
    jmp 0x08:.long_mode_64

; ─── 64-bit long mode ─────────────────────────────────────────────────────────
bits 64
.long_mode_64:
    ; Set data segments (SS must be valid; DS/ES/FS/GS are ignored in 64-bit code)
    mov ax, 0x10        ; kernel data64
    mov ss, ax
    mov ds, ax
    mov es, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax

    ; Set up the boot kernel stack (16-byte aligned)
    mov rsp, stack_top

    ; Load multiboot args into rdi, rsi (SysV AMD64 calling convention)
    mov edi, [rel mb_magic]   ; rdi = magic   (zero-extended from 32-bit)
    mov esi, [rel mb_info]    ; rsi = mbi_ptr (zero-extended from 32-bit)

    ; Call the C kernel
    call kernel_main

    ; If kernel_main returns, hang forever
    cli
.hang:
    hlt
    jmp .hang

; ─── Saved multiboot values ──────────────────────────────────────────────────
section .data
mb_magic: dd 0
mb_info:  dd 0
