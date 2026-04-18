# Compatibilidade com Binários Linux no Krypx

## Resumo

**Sim, é possível** executar binários Linux dentro do Krypx — e a base já está implementada.
A camada `compat/` traduz syscalls Linux i386 para chamadas internas do Krypx.
Mas o alcance depende do tipo de binário: ferramentas simples já funcionam hoje,
browsers exigem um nível de completude comparável ao Wine ou WSL2.

---

## O que já existe no Krypx

### Camada de tradução de syscalls (`compat/linux_compat.c`)

Quando o loader ELF detecta que um binário tem OSABI = 0 (System V / Linux),
ele marca o processo com `compat_mode = COMPAT_LINUX`. A partir daí,
toda `int 0x80` desse processo é desviada para `linux_syscall_handler()`
em vez do handler nativo Krypx.

Syscalls já traduzidas:

| Nº Linux | Nome              | Tradução no Krypx                          |
|----------|-------------------|--------------------------------------------|
| 1        | exit              | Marca processo como ZOMBIE, chama schedule |
| 3        | read              | `vfs_read()` + stdin retorna EOF           |
| 4        | write             | `vga_putchar()` ou callback do terminal    |
| 5        | open              | `vfs_resolve()` + abre FD no processo      |
| 6        | close             | `vfs_close()` + libera FD                 |
| 20       | getpid            | Retorna `process_t.pid`                    |
| 45       | brk               | `pmm_alloc_page()` + `vmm_map_page()`      |
| 192      | mmap2             | Aloca páginas físicas e mapeia no processo |
| 122      | uname             | Responde "Linux 5.15.0 / i686 / krypx"    |
| 252      | exit_group        | Mesmo que exit                             |
| 54,33... | ioctl/access/stat | Stubs que retornam 0 (silenciosos)         |

### Detecção automática de formato (`compat/detect.c`)

O loader inspeciona os magic bytes do executável:

- `0x7F ELF` com OSABI 0 ou 3 → **Linux ELF i386** → compat layer
- `0x7F ELF` com OSABI 0xFF → **Krypx nativo**
- `MZ` → **Windows PE** (stub, não executa)

---

## O que funciona hoje

Binários compilados com:

```bash
gcc -m32 -static -nostdlib -nostartfiles -fno-pie -e _start -o programa programa.c
```

Funcionam diretamente. Exemplos concretos:

- Programas de terminal que só usam `write()` e `exit()`
- Ferramentas que leem/escrevem arquivos via `open/read/write/close`
- Qualquer binário que use apenas as syscalls da tabela acima

---

## Níveis de Compatibilidade

### Nível 1 — Já funciona (binários `-nostdlib`)

Programas escritos diretamente sobre syscalls, sem libc.
São os menores e mais simples. Já rodam no Krypx hoje.

### Nível 2 — Viável com trabalho (binários `-static` com musl libc)

[musl](https://musl.libc.org/) é uma libc minimalista que compila estática e usa
pouquíssimas syscalls. Para suportá-la bastaria adicionar ~20 syscalls ao compat layer:

- `writev`, `readv`, `fcntl`, `dup`, `dup2`
- `gettimeofday`, `clock_gettime` (já tem stub)
- `sigaction`, `sigmask`, `rt_sigprocmask`
- `futex` (mutex/semáforos em userspace)
- `poll`, `select` (I/O multiplexing)
- `getcwd`, `chdir`, `unlink`, `mkdir`, `rename`
- `getuid`, `geteuid`, `getgid`, `getegid`

Com isso rodariam: `ls`, `cat`, `cp`, `grep`, `sed`, `awk`, `curl` (sem TLS),
editores como `nano` (sem ncurses), e basicamente qualquer tool de linha de comando
compilada estaticamente contra musl.

Também funcionaria o **BusyBox estático** — um único binário que emula >200 comandos
Unix. Seria o equivalente a ter um shell completo dentro do Krypx.

### Nível 3 — Difícil mas possível (glibc estática)

glibc é muito mais pesada e usa syscalls avançadas:

- `clone` (threads NPTL)
- `set_robust_list`, `set_tid_address`
- `mprotect`, `mremap`, `madvise`
- `epoll_create`, `epoll_ctl`, `epoll_wait`
- `inotify_*`, `timerfd_*`, `signalfd_*`
- `prctl` (controle de processo)
- `sched_getaffinity`, `sched_setscheduler`

Custo estimado: semanas de implementação. Mas viável.

### Nível 4 — Muito complexo (binários dinâmicos)

Praticamente todos os programas Linux "normais" usam `.so` (shared libraries).
Para suportá-los precisaria de:

1. **Dynamic linker** (`ld-linux.so.2` ou implementação própria)
2. **Sistema de shared libraries** — mapear múltiplos ELFs no mesmo espaço
3. **PLT/GOT resolution** em tempo de carga
4. **Versioning de símbolos** (compat com versões de glibc)

Isso é o que o **Wine** faz para executáveis Windows: reimplementou o loader
de DLLs PE inteiro. Para Linux seria análogo, mas tecnicamente mais simples
porque o formato ELF é aberto e bem documentado.

---

## E um Browser?

| Requisito do Browser          | Status no Krypx    | Esforço para adicionar |
|-------------------------------|---------------------|------------------------|
| Threads (clone/futex)         | Não tem             | Alto                   |
| Shared libraries (.so)        | Não tem             | Muito alto             |
| Sockets TCP/IP (já tem rede!) | Precisa conectar ao compat | Médio          |
| OpenGL / GPU acelerado        | Não tem             | Muito alto             |
| X11 ou Wayland para UI        | Não se aplica (usa framebuffer próprio) | —     |
| Sistema de fontes (fontconfig)| Não tem             | Médio                  |
| ALSA/PulseAudio (áudio)       | Não tem             | Alto                   |

**Conclusão sobre browsers**: não é viável no curto prazo com a arquitetura atual.
Um browser moderno (Firefox, Chromium) usa threads massivamente, linked dinâmicamente
a dezenas de `.so`, e precisa de GPU. Seria um projeto de meses/anos — o equivalente
a implementar um Wine para Linux dentro do Krypx.

**Alternativa realista**: um browser em texto como `links` ou `lynx`, compilado
estaticamente com musl. Para HTTP já funciona (o stack TCP/IP está implementado).
Para HTTPS precisaria de uma porta de mbedTLS ou BearSSL (pequenas bibliotecas TLS
que compilam sem threads).

---

## Caminho Recomendado

```
[Agora]        Binários -nostdlib funcionam ✓
               |
               v
[Próxima fase] Adicionar ~20 syscalls ao linux_compat.c
               → Suporte a musl libc estática
               → BusyBox roda (ls, cat, grep, shell...)
               |
               v
[Depois]       Implementar clone() + futex + sigaction
               → Programas com threads simples
               → curl, wget com TLS
               |
               v
[Longo prazo]  Dynamic linker próprio
               → Qualquer binário Linux i386 estático ou dinâmico
               → Equivalente a WSL1 (tradução de syscalls sem VM)
```

---

## Comparação com projetos similares

| Projeto   | O que faz                          | Analogia com Krypx              |
|-----------|------------------------------------|----------------------------------|
| **WSL1**  | Traduz syscalls Linux no Windows   | Mesma abordagem — mais completo  |
| **Wine**  | Traduz syscalls Windows no Linux   | Inverso — mesma arquitetura      |
| **FEX**   | Roda binários x86 em ARM           | Igual, mas cross-ISA             |
| **V86**   | VM x86 em JS — roda Linux inteiro  | Abordagem diferente (VM vs compat)|

O Krypx usa a mesma abordagem do WSL1: **sem VM, sem emulação de CPU**,
só tradução de syscalls. É o método mais eficiente — zero overhead de virtualização.

---

## Resumo Final

| Tipo de binário                    | Funciona hoje? | Com quanto trabalho?        |
|------------------------------------|----------------|-----------------------------|
| `-nostdlib` simples                | **Sim**        | Já implementado             |
| `-static` com musl libc            | Quase          | ~2 semanas (20 syscalls)    |
| BusyBox (shell + 200 comandos)     | Quase          | ~2 semanas                  |
| Ferramentas GNU clássicas estáticas| Não            | ~1-2 meses (glibc)          |
| Browser em texto (links/lynx)      | Quase          | ~3 semanas (musl + TLS)     |
| Browser gráfico (Firefox/Chromium) | Não            | Anos (threads + .so + GPU)  |
| Qualquer binário Linux dinâmico    | Não            | Meses (dynamic linker)      |
