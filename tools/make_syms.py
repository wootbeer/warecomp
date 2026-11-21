#!/usr/bin/env python3
"""Generate WASyms/wa.us.syms.toml (N64Recomp symbol file) for Worms Armageddon (USA).

Inputs:
  rom.z64             normalized ROM
  dis_dir             directory holding spimdisasm output for each code range,
                      in subfolders t_boot, t_front, t_game (see STATUS.md, round 1)
  sig_boot.txt        tools/sigmatch.py output for the boot segment (libultra names)

The ROM has a resident segment and two overlays that share one VRAM range
(STATUS.md round 1):
  boot   ROM 0x001000-0x038D40 -> 0x80000400
  front  ROM 0x038D40-0x07F0B0 -> 0x80048FE0 (menus)
  game   ROM 0x07F0B0-0x0E7090 -> 0x80048FE0 (the match)
"""
import re, sys, os, glob

SEGMENTS = [
    # name, section name, rom start, rom end (whole segment), vram, [code ranges as (rom_start, rom_end)]
    # boot's code skips ROM 0x16770-0x18D60 (0x80015B70-0x80018160): RSP microcode
    # text sits there (audio ucode, rspboot, graphics ucode; STATUS.md round 1)
    ("boot",  ".boot",  0x001000, 0x038D40, 0x80000400, [(0x1000, 0x1040), (0x3CB0, 0x16770), (0x18D60, 0x34FC8)]),
    ("front", ".front", 0x038D40, 0x07F0B0, 0x80048FE0, [(0x3E790, 0x7EABC)]),
    ("game",  ".game",  0x07F0B0, 0x0E7090, 0x80048FE0, [(0x83930, 0xE6C08)]),
    # Seven modules the frontend loads one at a time to 0x800B20F0, just past
    # the match overlay and its BSS (loader at 0x8004F574; STATUS.md round 4).
    # Each is a "ptab" header and data, then a block of ~216 leaf functions
    # (lui/jr/addiu getters returning the address of an item in the data).
    ("mod0", ".mod0", 0x0E81C0, 0x0F2500, 0x800B20F0, [(0x0E81C0 + 0x8C10, 0x0E81C0 + 0x9638)]),
    ("mod1", ".mod1", 0x0F2500, 0x0FD9D0, 0x800B20F0, [(0x0F2500 + 0x9D70, 0x0F2500 + 0xA7A4)]),
    ("mod2", ".mod2", 0x0FD9D0, 0x108700, 0x800B20F0, [(0x0FD9D0 + 0x95F0, 0x0FD9D0 + 0xA024)]),
    ("mod3", ".mod3", 0x108700, 0x113990, 0x800B20F0, [(0x108700 + 0x9AF0, 0x108700 + 0xA524)]),
    ("mod4", ".mod4", 0x113990, 0x11E560, 0x800B20F0, [(0x113990 + 0x9430, 0x113990 + 0x9E7C)]),
    ("mod5", ".mod5", 0x11E560, 0x129760, 0x800B20F0, [(0x11E560 + 0x9A60, 0x11E560 + 0xA4AC)]),
    ("mod6", ".mod6", 0x129760, 0x135070, 0x800B20F0, [(0x129760 + 0xA180, 0x129760 + 0xABB4)]),
]

# Hand corrections (vram -> True adds a function start, False removes one).
MANUAL = {
    # 0x800145E8 (a string routine) enters with `j 0x80014604`, the test at the
    # bottom of its own loop: not a separate function
    0x80014604: False, 0x8001461C: True,
    # hand-written leaf math routines (no stack frames) before the ucode
    0x80015B60: True,
}

# Hand-identified libultra routines (hand-written assembly with no signature
# match; N64Recomp skips or reimplements them by name). STATUS.md round 1.
NAMES = {
    0x8002ADC0: "osWritebackDCacheAll",   # cache 0x01 over 0x80000000-0x80002000
    0x8002B6F0: "__osExceptionPreamble",  # lui/addiu k0; jr k0 to 0x8002B700
    0x8002B700: "__osException",
    # osGetTime/osSetTimer wait, then __osPackRequestData, __osSiRawStartDma
    # and __osContGetInitData: the shape of osContInit (just before
    # __osContGetInitData); unnamed, it called helpers the runtime skips
    0x8002ADF0: "osContInit",
    # lw from 0xA4500004 (AI_LEN): osAiGetLength; too short for a signature
    0x80026FF0: "osAiGetLength",
    # SI_STATUS (0xA4800018) busy bits: a call-target name had it as
    # __osSpDeviceBusy, which is the one at 0x80034100 (SP_STATUS 0xA4040010)
    0x8002DC70: "__osSiDeviceBusy",
    0x80034100: "__osSpDeviceBusy",
    # the same as osViGetCurrentFramebuffer (0x80034240) but reading
    # __osViNext (0x80036C84): osViGetNextFramebuffer. Unnamed, it read
    # libultra's VI state, which the runtime doesn't keep, and the scheduler
    # held the third frame forever waiting for its buffer (round 3)
    0x80034280: "osViGetNextFramebuffer",
}

SMALL_OK = {"osAiGetStatus", "osSpTaskYield", "__osGetCause", "__osSetSR", "osScGetCmdQ",
            "osContSetCh", "osAfterPreNMI", "osPiGetCmdQueue"}

MANUAL_KEEP = {v for v, add in MANUAL.items() if add} | set(NAMES)

def branch_target(w, v):
    op = w >> 26
    if op in (4, 5, 6, 7, 20, 21, 22, 23):          # beq bne blez bgtz (+likely)
        return v + 4 + (((w & 0xFFFF) ^ 0x8000) - 0x8000) * 4, True
    if op == 1 and ((w >> 16) & 0x1F) in (0, 1, 2, 3):  # bltz bgez (+likely); not the *al forms
        return v + 4 + (((w & 0xFFFF) ^ 0x8000) - 0x8000) * 4, True
    if op == 0x11 and ((w >> 21) & 0x1F) == 8:       # bc1t/bc1f
        return v + 4 + (((w & 0xFFFF) ^ 0x8000) - 0x8000) * 4, True
    if op == 2:                                       # j
        return (v & 0xF0000000) | ((w & 0x3FFFFFF) << 2), False
    return None, None

def is_branch_or_jump(w):
    op = w >> 26
    if w == 0:
        return False
    if op in (2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or op == 1:   # j jal b* (+likely), regimm
        return True
    if op == 0 and (w & 0x3F) in (8, 9):                     # jr jalr
        return True
    return op == 0x11 and ((w >> 21) & 0x1F) == 8            # bc1*

def merge_crossing(starts, rng, word_at, protected, soft=frozenset()):
    absorbed = set()
    # Nothing starts in a delay slot: the instruction before it is the
    # branch or jump it belongs to.
    starts = [x for x in starts if not (rng(x - 4) and is_branch_or_jump(word_at(x - 4))) or x in protected]
    starts = sorted(starts)
    changed = True
    while changed:
        changed = False
        sset = set(starts)
        for i, st in enumerate(starts):
            a, b = rng(st)
            end = starts[i + 1] if i + 1 < len(starts) and starts[i + 1] < b else b
            for v in range(st, end, 4):
                t, cond = branch_target(word_at(v), v)
                if t is None or not (a <= t < b):
                    continue
                if not cond and t in sset:
                    continue  # tail call to a function start
                if st <= t < end:
                    continue
                # remove the starts between this function and the target
                lo, hi = (end, t) if t >= end else (t, st)
                drop = {x for x in starts if (lo <= x <= hi if t >= end else lo < x <= hi)}
                drop = {x for x in drop if x != st or t >= end}
                if t < st:
                    drop = {x for x in starts if t < x <= st}
                drop -= protected
                absorbed |= drop & soft
                if drop:
                    starts = [x for x in starts if x not in drop]
                    changed = True
                break
            if changed:
                break
    return starts, absorbed

def main():
    rom = open(sys.argv[1], 'rb').read()
    dis = sys.argv[2]
    sig = sys.argv[3]
    out = sys.argv[4]
    names = {}
    for line in open(sig):
        n, v, s = line.split()
        v = int(v, 16)
        s = int(s, 16)
        # Tiny signatures (a handful of instructions) match ordinary game code by
        # chance (__dummy, ptstart, n_alSeqpDelete... inside the engine); keep only
        # signatures long enough to be specific, plus names taken from call targets
        # inside matched functions (size 0 here: osInvalDCache, bzero, ...).
        if 0 < s < 0x24 and n not in SMALL_OK:
            continue
        # The game links the plain audio library (al*, matched directly);
        # n_al* names only come from call targets inside other signatures,
        # and landed on game code (0x80032D28 is a busy-wait, not
        # n_alLoadParam).
        if s == 0 and n.startswith("n_al"):
            continue
        names.setdefault(v, []).append((n, s))

    def word(r): return int.from_bytes(rom[r:r+4], 'big')
    lines = ["# Worms Armageddon (USA) (En,Fr,Es) symbol file for N64Recomp.",
             "# Generated by tools/make_syms.py, then corrected by hand where noted.",
             "# Function boundaries: spimdisasm; libultra/audio names: tools/sigmatch.py",
             "# (n64sym signature data). See STATUS.md.", ""]
    total = 0
    manual = []
    for seg, secname, rs, re_, vs, code in SEGMENTS:
        starts = set()
        for f in glob.glob(os.path.join(dis, "t_" + seg, "**", "*.s"), recursive=True):
            for l in open(f):
                m = re.search(r'(glabel|alabel)\s+(\w+)', l)
                if m:
                    pending = m.group(1)
                    continue
                m2 = re.search(r'/\* ([0-9A-F]+) ([0-9A-F]{8}) ([0-9A-F]{8}) \*/', l)
                if m2 and 'pending' in dir() and pending:
                    v = int(m2.group(2), 16)
                    if pending == 'glabel':
                        starts.add(v)
                    else:
                        # alabel: only a function start if it follows the end of one
                        r = int(m2.group(1), 16)
                        p = r - 4
                        while p > 0 and word(p) == 0: p -= 4
                        if word(p - 4) == 0x03E00008 or (word(p) >> 26) == 2:  # jr ra + delay, or j
                            starts.add(v)
                    pending = None
        # extra splits: a stack-frame prologue right after jr ra + delay slot (+ nop padding)
        for cs, ce in code:
            starts.add(vs + cs - rs)
            for r in range(cs, ce, 4):
                w = word(r)
                if (w >> 16) == 0x27BD and (w & 0x8000):
                    p = r - 4
                    while p > cs and word(p) == 0: p -= 4
                    # jr ra or j + delay slot (a nop delay slot is skipped above, landing on the jump)
                    if word(p - 4) == 0x03E00008 or word(p) == 0x03E00008 or (word(p - 4) >> 26) == 2 or (word(p) >> 26) == 2:
                        starts.add(vs + r - rs)
        if seg.startswith("mod"):
            for cs, ce in code:
                for r in range(cs, ce, 4):
                    if word(r) == 0x03E00008 and r + 8 < ce:
                        starts.add(vs + r + 8 - rs)
        if seg == "boot":
            for v in names:
                starts.add(v)
        if seg == "boot":
            # a matched signature gives the whole function: nothing starts inside it
            for v, lst in names.items():
                sz = max(z for _, z in lst)
                if sz:
                    starts = {x for x in starts if not (v < x < v + sz)}
                    e = v + sz
                    while word(rs + e - vs) == 0: e += 4
                    starts.add(e)
        # every jal target is a function start: from this segment's own code,
        # and from the boot segment's calls into the overlay range (the
        # loader calls the overlay it just loaded) when it looks like one here
        def plausible(r):
            p = r - 4
            while word(p) == 0: p -= 4
            w = word(p)
            return (word(r) >> 16) == 0x27BD or w == 0x03E00008 or word(p - 4) == 0x03E00008 or (word(p - 4) >> 26) == 2
        jal_targets = set()
        for cs, ce in code:
            for r in range(cs, ce, 4):
                w = word(r)
                if w >> 26 == 3:
                    t = 0x80000000 | ((w & 0x3FFFFFF) << 2)
                    for a, b in code:
                        if vs + a - rs <= t < vs + b - rs:
                            starts.add(t)
                            jal_targets.add(t)
        if seg == "boot":
            # calls from the overlays into the resident code
            for _, _, ors, _, ovs, ocode in SEGMENTS[1:]:
                for a, b in ocode:
                    for r in range(a, b, 4):
                        w = word(r)
                        if w >> 26 == 3:
                            t = 0x80000000 | ((w & 0x3FFFFFF) << 2)
                            for cs, ce in code:
                                if vs + cs - rs <= t < vs + ce - rs:
                                    starts.add(t)
                                    jal_targets.add(t)
        # Pointers in data (vtables, callback tables) to a plausible function
        # start in this segment's code: from this segment's own data, and for
        # boot from every segment's. A run of 3+ consecutive code pointers is
        # a switch jump table, whose targets are inside a function: skipped.
        def code_ptr(x):
            return any(vs + a - rs <= x < vs + b - rs for a, b in code)
        sources = [(rs, re_, code)] if seg != "boot" else [(o[2], o[3], o[5]) for o in SEGMENTS]
        for drs, dre, dcode in sources:
            r = drs
            while r < dre:
                if any(a <= r < b for a, b in dcode):
                    r += 4
                    continue
                run = r
                while run < dre and code_ptr(word(run)) and not any(a <= run < b for a, b in dcode):
                    run += 4
                n = (run - r) // 4
                if 1 <= n <= 2:
                    for q in range(r, run, 4):
                        t = word(q)
                        if plausible(rs + t - vs):
                            starts.add(t)
                r = run + 4 if n else r + 4
        if seg != "boot":
            for a, b in SEGMENTS[0][5]:
                for r in range(a, b, 4):
                    w = word(r)
                    if w >> 26 == 3:
                        t = 0x80000000 | ((w & 0x3FFFFFF) << 2)
                        for cs, ce in code:
                            if vs + cs - rs <= t < vs + ce - rs and plausible(rs + t - vs):
                                starts.add(t)
                                jal_targets.add(t)
        if seg == "boot":
            starts |= set(NAMES)
        # __osException runs to __osEnqueueAndYield (0x8002BD6C); it has
        # internal returns and a stack-frame-like store that read as splits
        starts = {x for x in starts if not (0x8002B700 < x < 0x8002BD6C)}
        for v, add in MANUAL.items():
            if add: starts.add(v)
            else: starts.discard(v)
        code_v = [(vs + a - rs, vs + b - rs) for a, b in code]
        def rng(v):
            for a, b in code_v:
                if a <= v < b: return (a, b)
        starts = sorted(s for s in starts if rng(s))
        # Merge pass: a branch (or a `j` to somewhere that isn't a function
        # start) from one function into another means the boundary between
        # them is wrong. Merge until no branch leaves its function.
        # A call target that another function also jumps into (shared tail
        # code) is merged into that function and registered again on its own
        # as an overlapping entry of its own in the same section (N64Recomp
        # only applies manual_funcs to ELF input, not to a symbol file).
        extra_entries = []
        starts, absorbed = merge_crossing(starts, rng, lambda v: word(rs + v - vs),
                                          protected=set(MANUAL_KEEP), soft=jal_targets)
        for x in sorted(absorbed):
            a, b = rng(x)
            nxt = [y for y in starts if y > x and y < b]
            end = nxt[0] if nxt else b
            manual.append((secname, x, end - x, seg))
            extra_entries.append((x, end - x))
        lines += ["[[section]]", f'name = "{secname}"', f"rom = 0x{rs:06X}", f"vram = 0x{vs:08X}",
                  f"size = 0x{re_ - rs:X}", "", "functions = ["]
        used = set()
        for i, s in enumerate(starts):
            a, b = rng(s)
            e = starts[i + 1] if i + 1 < len(starts) and starts[i + 1] < b else b
            # trim trailing nop padding
            while e - 4 > s and word(rs + (e - 4 - vs)) == 0 and word(rs + (e - 8 - vs)) != 0x03E00008 and (word(rs + (e - 8 - vs)) >> 26) not in (2,) and word(rs + (e - 8 - vs)) == 0:
                e -= 4
            name = NAMES.get(s, f"func_{s:08X}")
            if seg == "boot" and s in names and s not in NAMES:
                cand = [n for n, sz in names[s] if n not in used and not n.startswith('_') or n in ('__osDisableInt','__osRestoreInt')]
                cand = [n for n, sz in names[s] if n not in used]
                good = [n for n in cand if '_text_' not in n and '_rodata_' not in n and '_data_' not in n]
                if good:
                    name = good[0]
            elif seg != "boot":
                name = f"{seg}_{s:08X}"
            used.add(name)
            lines.append(f'    {{ name = "{name}", vram = 0x{s:08X}, size = 0x{e - s:X} }},')
            total += 1
        for x, sz in extra_entries:
            nm = NAMES.get(x) or (f"func_{x:08X}" if seg == "boot" else f"{seg}_{x:08X}")
            lines.append(f'    {{ name = "{nm}", vram = 0x{x:08X}, size = 0x{sz:X} }},  # also the tail of the function before it')
            total += 1
        lines += ["]", ""]
    open(out, 'w').write("\n".join(lines))
    print("shared-tail entries:", len(manual))
    print("functions:", total)

main()
