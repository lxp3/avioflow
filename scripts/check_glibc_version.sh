#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "Usage: $0 ELF_FILE [...]" >&2
  exit 2
fi

max_allowed=2.28
for file in "$@"; do
  test -f "$file" || { echo "ELF file not found: $file" >&2; exit 1; }
  versions=$(readelf --version-info "$file" | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | cut -d_ -f2 | sort -Vu || true)
  highest=$(printf '%s\n' "$versions" | tail -n 1)
  if [ -n "$highest" ] && [ "$(printf '%s\n%s\n' "$max_allowed" "$highest" | sort -V | tail -n 1)" != "$max_allowed" ]; then
    echo "$file requires GLIBC_$highest, newer than GLIBC_$max_allowed" >&2
    exit 1
  fi
  echo "$file: highest required GLIBC_${highest:-none}"
done
