#!/usr/bin/env python3
"""Tiny Microtronic assembler. Labels -> addresses; emits 256-word program."""
import sys, re

REG2 = {'MOV':0,'AND':2,'ADD':4,'SUB':6,'CMP':8,'OR':0xA}      # op s,d  -> o s d
IMM2 = {'MOVI':1,'ANDI':3,'ADDI':5,'SUBI':7,'CMPI':9}          # op n,r  -> o n r
JMP  = {'CALL':0xB,'GOTO':0xC,'BRC':0xD,'BRZ':0xE}             # op label
F0   = {'RET':0xF07,'CLEAR':0xF08,'HXDZ':0xF03,'DZHX':0xF04,'MULT':0xF0B,
        'DIV':0xF0C,'EXRL':0xF0D,'EXRM':0xF0E,'RND':0xF05,'NOP':0xF01,
        'HALT':0xF00,'STC':0xF09,'RSC':0xF0A,'DISOUT':0xF02}
F1   = {'INV':0xF8,'SHR':0xF9,'SHL':0xFA,'DOT':0xFE,'KIN':0xFF,'DIN':0xFD,
        'MAS':0xF7,'ADC':0xFB,'SUBC':0xFC}                     # op r -> Fxr

def h(x): return int(x,16)

def assemble(src):
    # pass 1: tokenize into (label, mnemonic, args)
    items=[]
    for raw in src.splitlines():
        line=raw.split(';')[0].split('#')[0].strip()
        if not line: continue
        lbl=None
        m=re.match(r'^(\w+):\s*(.*)$',line)
        if m: lbl=m.group(1); line=m.group(2).strip()
        if line=='' and lbl: items.append((lbl,None,None)); continue
        parts=line.split(None,1)
        mn=parts[0].upper(); args=parts[1].strip() if len(parts)>1 else ''
        items.append((lbl,mn,args))
    # pass 1b: assign addresses
    addr=0; sym={}; words=[]  # words: list of (addr, kind, payload)
    for lbl,mn,args in items:
        if lbl: sym[lbl]=addr
        if mn is None: continue
        if mn=='.ORG': addr=h(args);
        # count size = 1 for every emitting instruction (incl RAW); .ORG doesn't emit
        if mn=='.ORG':
            continue
        words.append((addr,mn,args)); addr+=1
    # pass 2: emit
    out={}
    for a,mn,args in words:
        A=[x.strip() for x in args.split(',')] if args else []
        # NB: check mnemonics BEFORE raw-literals — "ADD"/"ADC" are also valid 3-hex strings!
        if mn=='RAW': w=h(A[0])
        elif mn in REG2: w=(REG2[mn]<<8)|(h(A[0])<<4)|h(A[1])
        elif mn in IMM2: w=(IMM2[mn]<<8)|(h(A[0])<<4)|h(A[1])
        elif mn in JMP:
            t=A[0]; t=sym[t] if t in sym else h(t)
            w=(JMP[mn]<<8)|(t&0xFF)
        elif mn in F0: w=F0[mn]
        elif mn in F1: w=F1[mn]<<4 | h(A[0])
        elif mn=='DISP': w=0xF00|(h(A[0])<<4)|h(A[1])
        elif re.fullmatch(r'[0-9A-Fa-f]{3}',mn):       # raw literal data word (last resort)
            w=h(mn)
        else: raise SystemExit('bad mnemonic %s @%02X'%(mn,a))
        if a in out: raise SystemExit('overlap @%02X'%a)
        out[a]=w
    return out,sym

def to_mic(out,header=''):
    last=max(out) if out else 0
    lines=[header.rstrip(),'@ 00','']
    for a in range(last+1):
        lines.append('%03X'%out.get(a,0))   # gaps -> 000
    return '\n'.join(lines)+'\n'

if __name__=='__main__':
    src=open(sys.argv[1]).read()
    out,sym=assemble(src)
    open(sys.argv[2],'w').write(to_mic(out,'# assembled'))
    print('assembled %d words, last=%02X'%(len(out),max(out)))
    for k,v in sorted(sym.items(),key=lambda kv:kv[1]):
        print('  %02X %s'%(v,k))
