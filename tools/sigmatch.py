"""Match n64sym-format signatures (.sig) against a .z64 ROM segment.
Usage: sigmatch.py rom.z64 sigfile rom_start rom_end vram_start
Prints: name vram size  (functions), plus reloc-derived names with size 0."""
import sys, zlib, collections
rom=open(sys.argv[1],'rb').read()
sigpath=sys.argv[2]; rs=int(sys.argv[3],16); re_=int(sys.argv[4],16); vs=int(sys.argv[5],16)
TYPES={'.targ26':4,'.hi16':5,'.lo16':6}
syms=[]
cur=None
for line in open(sigpath):
    line=line.strip()
    if not line or line.startswith('#'): continue
    p=line.split()
    if p[0] in TYPES:
        for o in p[2:]: cur['relocs'].append((int(o,16),TYPES[p[0]],p[1]))
    else:
        cur={'name':p[0],'size':int(p[1],16),'a':int(p[2],16),'b':int(p[3],16),'relocs':[]}
        syms.append(cur)
for s in syms: s['relocs'].sort()
def strip(word,t):
    if t==4: return word & 0xFC000000
    return word & 0xFFFF0000
def stripped_bytes(off,size,relocs):
    buf=bytearray(rom[off:off+size])
    for ro,t,_ in relocs:
        if ro+4<=size:
            w=int.from_bytes(buf[ro:ro+4],'big'); buf[ro:ro+4]=strip(w,t).to_bytes(4,'big')
    return bytes(buf)
seg=rom[rs:re_]
cands=set()
for i in range(rs,re_-8,4):
    w=int.from_bytes(rom[i:i+4],'big')
    if w==0x03E00008 and rom[i+8:i+12]!=b'\0\0\0\0': cands.add(i+8)
    if (w>>16)==0x27BD and (w&0x8000): cands.add(i)
cands.add(rs)
cands=sorted(c for c in cands if c<re_)
# group symbols by crcA strip pattern
groups=collections.defaultdict(list)
for s in syms:
    lim=min(s['size'],8)
    pat=tuple((o,t) for o,t,_ in s['relocs'] if o<lim)
    groups[(lim,pat)].append(s)
found={}
names_used=set()
for (lim,pat),ss in groups.items():
    idx=collections.defaultdict(list)
    for c in cands:
        b=bytearray(rom[c:c+lim])
        for o,t in pat:
            w=int.from_bytes(b[o:o+4],'big'); b[o:o+4]=strip(w,t).to_bytes(4,'big')
        idx[zlib.crc32(bytes(b))].append(c)
    for s in ss:
        for c in idx.get(s['a'],()):
            if c+s['size']>re_: continue
            if zlib.crc32(stripped_bytes(c,s['size'],s['relocs']))==s['b']:
                if s['name'] in names_used: continue
                names_used.add(s['name'])
                found.setdefault(c,[]).append(s)
                break
out=[]
for c in sorted(found):
    s=max(found[c],key=lambda s:s['size'])
    out.append((vs+c-rs,s['name'],s['size']))
    # reloc targets
    hi={}
    for ro,t,n in s['relocs']:
        w=int.from_bytes(rom[c+ro:c+ro+4],'big')
        if t==4: out.append(((vs&0xF0000000)|((w&0x3FFFFFF)<<2),n,0))
for v,n,sz in sorted(set(out)):
    print(f"{n} 0x{v:08X} 0x{sz:X}")
