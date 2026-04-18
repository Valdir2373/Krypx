#!/usr/bin/env bash
# scripts/mkpkg.sh — Cria arquivo .kpkg a partir de um diretório
#
# Uso: bash scripts/mkpkg.sh <dir> <saida.kpkg>
# Formato:
#   "KPKG" (4 bytes)
#   version uint32_le = 1
#   n_files uint32_le
#   Para cada arquivo:
#     path   256 bytes (null-padded)
#     mode   uint32_le
#     size   uint32_le
#     data   <size> bytes

set -euo pipefail

SRC="${1:?Uso: $0 <diretorio> <saida.kpkg>}"
OUT="${2:?Uso: $0 <diretorio> <saida.kpkg>}"

if [ ! -d "$SRC" ]; then
    echo "ERRO: $SRC nao e um diretorio" >&2
    exit 1
fi

# Write little-endian uint32
write_u32() {
    local v=$1
    printf "\\x$(printf '%02x' $((v & 0xFF)))"
    printf "\\x$(printf '%02x' $(((v >> 8) & 0xFF)))"
    printf "\\x$(printf '%02x' $(((v >> 16) & 0xFF)))"
    printf "\\x$(printf '%02x' $(((v >> 24) & 0xFF)))"
}

# Write null-padded string of fixed length
write_str() {
    local s="$1"
    local len="$2"
    local slen=${#s}
    printf '%s' "$s"
    local pad=$((len - slen))
    for ((i=0; i<pad; i++)); do printf '\x00'; done
}

# Collect files
mapfile -t FILES < <(find "$SRC" -type f | sort)
N="${#FILES[@]}"

echo "[MKPKG] $N arquivos em $SRC"

{
    # Header
    printf 'KPKG'           # magic
    write_u32 1              # version
    write_u32 "$N"           # n_files

    for f in "${FILES[@]}"; do
        rel="${f#$SRC/}"
        # Prepend / if not already absolute path in the archive
        path="/$rel"
        size=$(stat -c%s "$f")
        mode=$(stat -c%a "$f")  # octal permissions
        mode_dec=$((8#$mode))   # convert to decimal

        write_str "$path" 256
        write_u32 "$mode_dec"
        write_u32 "$size"
        cat "$f"

        echo "  + $path ($size bytes)" >&2
    done
} > "$OUT"

echo "[MKPKG] Criado: $OUT ($(stat -c%s "$OUT") bytes)"
