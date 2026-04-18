#ifndef _KPKG_H
#define _KPKG_H

/*
 * kpkg — Krypx Package Manager
 * Package format (.kpkg = simple archive):
 *   magic:    "KPKG" (4 bytes)
 *   version:  uint32_t (1)
 *   n_files:  uint32_t
 *   files[]:
 *     path:   256 bytes (null-terminated)
 *     mode:   uint32_t
 *     size:   uint32_t
 *     data:   size bytes
 */

#define KPKG_MAGIC      "KPKG"
#define KPKG_VERSION    1
#define KPKG_PATH_MAX   256
#define KPKG_DB_DIR     "/var/db/kpkg"
#define KPKG_PKG_DIR    "/packages"

typedef void (*kpkg_print_fn)(const char *s);

/* Install a .kpkg archive from the given VFS path.
 * Calls print(msg) for progress/error output.
 * Returns 0 on success. */
int kpkg_install(const char *pkgpath, kpkg_print_fn print);

/* List installed packages (reads /var/db/kpkg/). */
void kpkg_list(kpkg_print_fn print);

/* Search for available packages in /packages/. */
void kpkg_search(const char *name, kpkg_print_fn print);

/* Create a .kpkg archive from a directory tree (host-side utility, no-op in kernel). */
int kpkg_create(const char *srcdir, const char *outpath, kpkg_print_fn print);

#endif
