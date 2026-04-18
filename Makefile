# Makefile — Sistema de build do Krypx (x86_64)

CC      := gcc
LD      := ld
AS      := nasm
OBJCOPY := objcopy
ISO     := grub2-mkrescue
QEMU    := qemu-system-x86_64
KVM_FLAG := $(shell [ -e /dev/kvm ] && echo "-enable-kvm" || echo "-accel tcg,tb-size=32")

# ============================================================
# Flags
# ============================================================
CFLAGS := -m64 -mno-red-zone -mno-sse -mno-mmx -mcmodel=small \
          -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
          -fno-exceptions -fno-stack-protector -fno-builtin \
          -fno-pie -fno-pic \
          -nostdlib -nostdinc \
          -I. -Iinclude -Ilib -Idrivers -Ifs -Imm -Iproc -Inet -Igui -Isecurity -Iapps -Icompat

ASFLAGS := -f elf64

LDFLAGS := -T linker.ld -nostdlib -m elf_x86_64 -z max-page-size=0x1000

# ============================================================
# Arquivos fonte
# ============================================================
ASM_SOURCES := boot/boot.asm \
               boot/gdt.asm \
               boot/idt.asm \
               boot/isr.asm \
               boot/switch.asm \
               boot/syscall_entry.asm

C_SOURCES   := compat/detect.c \
               compat/linux_compat.c \
               compat/linux_compat64.c \
               compat/win_compat.c \
               proc/elf.c \
               proc/dynlink.c \
               kernel/kernel.c \
               kernel/gdt.c \
               kernel/idt.c \
               kernel/timer.c \
               drivers/vga.c \
               drivers/keyboard.c \
               drivers/mouse.c \
               drivers/ide.c \
               drivers/pci.c \
               drivers/e1000.c \
               mm/pmm.c \
               mm/vmm.c \
               mm/heap.c \
               lib/string.c \
               fs/vfs.c \
               fs/fat32.c \
               fs/devfs.c \
               fs/procfs.c \
               proc/process.c \
               proc/scheduler.c \
               kernel/syscall.c \
               drivers/framebuffer.c \
               gui/canvas.c \
               gui/window.c \
               gui/desktop.c \
               gui/x11_server.c \
               net/net.c \
               net/ethernet.c \
               net/arp.c \
               net/ip.c \
               net/icmp.c \
               net/udp.c \
               net/tcp.c \
               net/dhcp.c \
               net/dns.c \
               net/socket.c \
               net/netif.c \
               security/auth.c \
               security/users.c \
               security/permissions.c \
               security/aslr.c \
               apps/calculator.c \
               apps/task_manager.c \
               apps/about.c \
               apps/network_manager.c \
               apps/settings.c \
               apps/file_manager.c \
               apps/text_editor.c \
               apps/image_viewer.c \
               apps/kpkg.c \
               lib/png.c \
               lib/jpeg.c \
               lib/sha1.c \
               lib/aes.c \
               drivers/ac97.c \
               drivers/acpi.c \
               drivers/ahci.c \
               drivers/usb_hid.c \
               drivers/rtl8188eu.c \
               net/wifi.c \
               net/wpa2.c \
               gui/clipboard.c

# Objetos gerados
ASM_OBJECTS := $(ASM_SOURCES:.asm=.o)
C_OBJECTS   := $(C_SOURCES:.c=.o)
ALL_OBJECTS := $(ASM_OBJECTS) $(C_OBJECTS)

# ============================================================
# Alvos principais
# ============================================================
.PHONY: all iso run run-debug clean kill-qemu

all: kernel.bin

# Linka o kernel
kernel.bin: $(ALL_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJECTS)
	@echo "[LD]  kernel.bin gerado"
	@size $@

# Gera a ISO bootável
iso: kernel.bin
	@mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
	cp grub.cfg iso/boot/grub/grub.cfg
	$(ISO) --modules="fat iso9660 part_msdos part_gpt search" -o Krypx.iso iso/
	@echo "[ISO] Krypx.iso gerada"
	@ls -lh Krypx.iso

# Mata instâncias antigas antes de iniciar
kill-qemu:
	@-pkill -f "qemu-system-x86_64" 2>/dev/null || true
	@sleep 0.3

# Roda no QEMU (modo normal)
run: iso kill-qemu
	$(QEMU) -cdrom Krypx.iso -m 512M \
	    -vga std \
	    -boot d \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown \
	    $(KVM_FLAG)

# Roda com rede (e1000)
run-net: iso kill-qemu
	$(QEMU) -cdrom Krypx.iso -m 512M \
	    -vga std \
	    -boot d \
	    -serial stdio \
	    -netdev user,id=net0 -device e1000,netdev=net0 \
	    -no-reboot \
	    -no-shutdown \
	    $(KVM_FLAG)

# Roda com debug GDB
run-debug: iso kill-qemu
	$(QEMU) -cdrom Krypx.iso -m 512M \
	    -vga std \
	    -boot d \
	    -serial stdio \
	    -s -S \
	    -no-reboot \
	    -no-shutdown

# Roda sem ISO (mais rápido para desenvolvimento)
run-kernel: kernel.bin kill-qemu
	$(QEMU) -kernel kernel.bin -m 512M \
	    -vga std \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown \
	    $(KVM_FLAG)

# ============================================================
# Regras de compilação
# ============================================================

# Assembly (NASM)
%.o: %.asm
	@echo "[AS]  $<"
	$(AS) $(ASFLAGS) -o $@ $<

# C
%.o: %.c
	@echo "[CC]  $<"
	$(CC) $(CFLAGS) -c -o $@ $<

# ============================================================
# Binário de teste Linux (i386 para compat layer)
# ============================================================
test-linux-binary:
	@echo "[CC]  tools/test_linux (Linux i386 static, no libc)"
	gcc -m32 -static -nostdlib -nostartfiles \
	    -fno-pie -fno-pic -Os \
	    -e _start \
	    -o tools/test_linux tools/test_linux.c
	strip tools/test_linux
	@echo "[OK]  tools/test_linux gerado ($$(stat -c%s tools/test_linux) bytes)"

# Cria disk.img FAT32 (2 GB para comportar Firefox + deps)
disk.img:
	@echo "[DISK] Criando disk.img (2 GB, FAT32)..."
	dd if=/dev/zero of=disk.img bs=1M count=2048 status=none
	mkdosfs -F32 disk.img
	@echo "[OK]  disk.img criado (2 GB)"

# ============================================================
# Firefox — download e instala no disk.img via Alpine Linux
# ============================================================
# Pré-requisitos no host: curl mtools bash
# Uso: make install-firefox   (precisa de disk.img criado)

FIREFOX_SCRIPT     := scripts/install_firefox.sh
ALPINE_BASE_SCRIPT := scripts/install_alpine_base.sh

# ============================================================
# Alpine Linux Subsystem — instala rootfs mínimo no disk.img
# ============================================================
# Isso cria o "Linux interno" do Krypx (estilo WSL).
# Depois de instalar, o terminal do Krypx abre /bin/sh do Alpine.
# Use: apk add firefox / apk add python3 / wget / etc.
#
# Pré-requisitos no host: curl, mtools, tar
install-alpine: disk.img
	@echo "[ALPINE] Instalando Alpine Linux Subsystem no disk.img..."
	@bash $(ALPINE_BASE_SCRIPT) disk.img
	@echo "[ALPINE] Pronto! Execute 'make run-linux' para testar."

# Roda Krypx com o Linux Subsystem (disco + rede)
run-linux: iso disk.img
	$(QEMU) -cdrom Krypx.iso -m 512M \
	    -vga std \
	    -boot d \
	    -serial stdio \
	    -netdev user,id=net0 -device e1000,netdev=net0 \
	    -drive file=disk.img,format=raw,if=ide \
	    -no-reboot \
	    -no-shutdown \
	    $(KVM_FLAG)

FIREFOX_SCRIPT := scripts/install_firefox.sh

install-firefox: disk.img
	@echo "[FIREFOX] Instalando Firefox + dependencias..."
	@bash $(FIREFOX_SCRIPT) disk.img
	@echo "[FIREFOX] Pronto! Execute 'make run-linux' e digite 'apk add firefox'."

# Roda com disco + rede (para Firefox)
run-firefox: iso disk.img
	$(QEMU) -cdrom Krypx.iso -m 1024M \
	    -vga std \
	    -boot d \
	    -serial stdio \
	    -netdev user,id=net0 -device e1000,netdev=net0 \
	    -drive file=disk.img,format=raw,if=ide \
	    -no-reboot \
	    -no-shutdown \
	    $(KVM_FLAG)

install-test: test-linux-binary disk.img
	@echo "[DISK] Instalando test_linux no disco (via mtools)..."
	mcopy -i disk.img -o tools/test_linux ::test_linux
	@echo "[OK]  /test_linux instalado no disk.img"

# Roda com disco (para testar o compat layer)
run-compat: iso disk.img
	$(QEMU) -cdrom Krypx.iso -m 512M \
	    -vga std \
	    -boot d \
	    -serial stdio \
	    -drive file=disk.img,format=raw,if=ide \
	    -no-reboot \
	    -no-shutdown \
	    $(KVM_FLAG)

# ============================================================
# Limpeza
# ============================================================
clean:
	rm -f $(ALL_OBJECTS) kernel.bin Krypx.iso
	rm -f iso/boot/kernel.bin
	@echo "[CLEAN] Feito"

clean-all: clean
	rm -f tools/test_linux disk.img
	@echo "[CLEAN] Tudo limpo"
