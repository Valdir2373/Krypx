#ifndef _KPKG_H
#define _KPKG_H

#include <types.h>

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
 *
 * Repository file (/etc/kpkg/repos):
 *   One URL per line: http://host/path/
 *   kpkg update downloads /index.kpkg from each repo
 *   Index lists: pkgname version url  (tab-separated)
 */

#define KPKG_MAGIC      "KPKG"
#define KPKG_VERSION    1
#define KPKG_PATH_MAX   256
#define KPKG_DB_DIR     "/var/db/kpkg"
#define KPKG_PKG_DIR    "/packages"
#define KPKG_REPOS_FILE "/etc/kpkg/repos"
#define KPKG_INDEX_FILE "/var/db/kpkg/index"
#define KPKG_DL_CHUNK   4096

typedef void (*kpkg_print_fn)(const char *s);

/* Install a .kpkg archive from the given VFS path. */
int kpkg_install(const char *pkgpath, kpkg_print_fn print);

/* Download a .kpkg from an HTTP URL and install it.
 * URL format: http://host[:port]/path/to/pkg.kpkg */
int kpkg_fetch(const char *url, kpkg_print_fn print);

/* Download package index from configured repos (/etc/kpkg/repos). */
int kpkg_update(kpkg_print_fn print);

/* Search package name: checks installed db, then index. */
void kpkg_search(const char *name, kpkg_print_fn print);

/* List installed packages. */
void kpkg_list(kpkg_print_fn print);

/* Remove an installed package (marks as uninstalled in db). */
int kpkg_remove(const char *name, kpkg_print_fn print);

/* Create a .kpkg archive from a directory (host-side only, no-op in kernel). */
int kpkg_create(const char *srcdir, const char *outpath, kpkg_print_fn print);

/* Parse http://host[:port]/path into components.
 * Returns 0 on success. host_out and path_out must be 256 bytes each. */
int kpkg_parse_url(const char *url, char *host_out, uint16_t *port_out, char *path_out);

#endif
