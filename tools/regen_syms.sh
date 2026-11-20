#!/usr/bin/env bash
# Regenerates WASyms/wa.us.syms.toml from the ROM (STATUS.md round 1).
#
#   tools/regen_syms.sh path/to/n64sym/src/builtin_signatures.sig
#
# Needs: python3 with spimdisasm and rabbitizer (pip install spimdisasm), the
# normalized ROM at the repo root, and n64sym's signature data file
# (https://github.com/shygoo/n64sym, src/builtin_signatures.sig; only the
# data file is read, by tools/sigmatch.py). Hand corrections live in
# tools/make_syms.py (MANUAL, NAMES), so they survive regeneration.
set -euo pipefail
cd "$(dirname "$0")/.."
sig="${1:?usage: tools/regen_syms.sh path/to/builtin_signatures.sig}"
rom="Worms Armageddon (USA).z64"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# code ranges per segment (tools/make_syms.py SEGMENTS)
disasm() { mkdir -p "$work/dis/t_$1"; python3 -m spimdisasm.singleFileDisasm "$rom" "$work/dis/t_$1/$4" --start "$2" --end "$3" --vram "$5" >/dev/null; }
disasm boot  0x3CB0  0x16770 a 0x800030B0
disasm boot  0x18D60 0x34FC8 b 0x80018160
disasm front 0x3E790 0x7EABC a 0x8004EA30
disasm game  0x83930 0xE6C08 a 0x8004D850
python3 tools/sigmatch.py "$rom" "$sig" 1000 38D40 80000400 > "$work/sig_boot.txt"
python3 tools/make_syms.py "$rom" "$work/dis" "$work/sig_boot.txt" WASyms/wa.us.syms.toml
