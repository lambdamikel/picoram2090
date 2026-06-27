import sys
def load(p):
    prog={};a=0;st=False
    for ln in open(p).read().splitlines():
        s=ln.strip()
        if s.startswith('@'): a=int(s.split()[1],16);st=True;continue
        if not st or s=='' or s.startswith('#'): continue
        prog[a]=int(s,16);a+=1
    return prog
# words to consume per ext mode (data words). sound=2,char=2,cursor=2,clearln=1,line=8
ARGN={0x6:2,0x8:2,0xA:8,0xD:2}     # keyed by l_op3 of 50x (506->6 char,508->8 cursor,50A->A line,50D->D sound)
IMMED={0x2,0x3,0x4,0x0,0x1}        # 502 clear,503 toggle,504 refresh,500,501: immediate (0 data words)
def run(prog,keys,maxsteps=400000):
    R=[0]*16;PC=0;RA=0;Z=0;C=0;ki=0
    ext_pending=0          # data words still expected by current ext opcode
    ext_name='-'
    desync=[]; reg_hijack=[]
    for step in range(maxsteps):
        if PC not in prog: return('FELLOFF',PC,step,desync,reg_hijack)
        w=prog[PC];o=w>>8;n1=(w>>4)&0xF;n2=w&0xF;nxt=PC+1
        # ---- PicoRAM bus watching (happens regardless of CPU effect) ----
        if o==5 and n1==0:                      # 50x extended opcode
            if n2 in IMMED: ext_pending=0; ext_name='imm%X'%n2
            elif n2 in ARGN: ext_pending=ARGN[n2]; ext_name='ext%X'%n2
        elif o==0 and n1==n2:                    # 0xx equal-nibble = data word
            if ext_pending>0: ext_pending-=1
            else: desync.append((PC,w,ext_name))   # stray data word consumed in wrong context
        elif o==3 and n1==0xF:                    # 3Fx register-operand hijack
            reg_hijack.append((PC,w))
            if ext_pending>0: ext_pending-=1
            else: desync.append((PC,w,'3Fx-standalone'))
        # ---- CPU execution ----
        if o==0: R[n2]=R[n1]
        elif o==1: R[n2]=n1
        elif o==2: R[n2]&=R[n1]
        elif o==3: R[n2]&=n1
        elif o==5:
            if not (n1==0):                      # 5,0,x handled above as ext; else ADDI
                v=R[n2]+n1;C=1 if v>15 else 0;R[n2]=v&15
        elif o==6: v=R[n2]-R[n1];C=1 if v<0 else 0;R[n2]=v&15
        elif o==7: v=R[n2]-n1;C=1 if v<0 else 0;R[n2]=v&15
        elif o==8: Z=1 if R[n2]==R[n1] else 0
        elif o==9: Z=1 if R[n2]==n1 else 0
        elif o==0xA: R[n2]|=R[n1]
        elif o==0xB: RA=PC+1;nxt=(n1<<4)|n2
        elif o==0xC: nxt=(n1<<4)|n2
        elif o==0xD: nxt=((n1<<4)|n2) if C else PC+1
        elif o==0xE: nxt=((n1<<4)|n2) if Z else PC+1
        elif o==0xF:
            if w==0xF07: nxt=RA
            elif w==0xF08: R=[0]*16
            elif w==0xF05: R[0xD],R[0xE],R[0xF]=3,5,2
            elif n1==0xF:
                if ki<len(keys): R[n2]=keys[ki];ki+=1
                else: return('NEEDKEY',PC,step,desync,reg_hijack)
        PC=nxt&0xFF
    return('MAXSTEPS',PC,maxsteps,desync,reg_hijack)

for tag,path in [('NEW(fixed)','/tmp/worig/PICO_WIPEON_SND.MIC'),
                 ('OLD(CALLBEEP)','/tmp/wipe/PICO_WIPEON_SND.MIC')]:
    r=run(load(path),[5])
    print('%-14s -> %-8s PC=%02X steps=%d  desyncs=%s  reg_hijacks=%s'%(
        tag,r[0],r[1],r[2],
        ([ '%02X:%03X(%s)'%(a,w,m) for a,w,m in r[3]] or 'none'),
        ([ '%02X:%03X'%(a,w) for a,w in r[4]] or 'none')))
