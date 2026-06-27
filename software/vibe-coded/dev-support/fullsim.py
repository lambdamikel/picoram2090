import re,sys
def load_mic(p):
    prog={};a=0;st=False
    for l in open(p).read().splitlines():
        s=l.strip()
        if s.startswith('@'): a=int(s.split()[1],16);st=True;continue
        if not st or s=='' or s.startswith('#'): continue
        prog[a]=int(s,16);a+=1
    return prog
def load_lst(p):
    prog={}
    for ln in open(p):
        m=re.match(r'^([0-9A-Fa-f]{2}):\s+([0-9A-Fa-f]{3})\b',ln)
        if m: prog[int(m.group(1),16)]=int(m.group(2),16)
    return prog
def bcd(rs): return sum((rs[i]&0xF)*(10**i) for i in range(len(rs)))
def run(prog,keys,maxsteps=60000,trace=False):
    R=[0]*16; EX=[0]*16; PC=0;RA=0;Z=0;C=0;ki=0;seq=[]
    for st in range(maxsteps):
        if PC not in prog: seq.append('FELL@%02X'%PC);break
        if (prog.get(PC,0)>>8)==0xF and ((prog[PC]>>4)&0xF)==0xF and (prog[PC]&0xF)==9:
            seq.append('KIN')
        if PC==0x8D: seq.append('c%d%s'%(R[7],'X' if R[6]==1 else 'O'))
        w=prog[PC];o=w>>8;n1=(w>>4)&0xF;n2=w&0xF;nx=PC+1
        if o==0:R[n2]=R[n1]
        elif o==1:R[n2]=n1
        elif o==2:R[n2]&=R[n1]
        elif o==3:R[n2]&=n1
        elif o==5:
            if n1: v=R[n2]+n1;C=1 if v>15 else 0;R[n2]=v&15
            # 50x extended ops: no register effect in this CPU sim
        elif o==6:v=R[n2]-R[n1];C=1 if v<0 else 0;R[n2]=v&15
        elif o==7:v=R[n2]-n1;C=1 if v<0 else 0;R[n2]=v&15
        elif o==8:Z=1 if R[n2]==R[n1] else 0; C=1 if R[n2]<R[n1] else 0
        elif o==9:Z=1 if R[n2]==n1 else 0; C=1 if R[n2]<n1 else 0
        elif o==0xA:R[n2]|=R[n1]
        elif o==0xB:RA=PC+1;nx=(n1<<4)|n2
        elif o==0xC:nx=(n1<<4)|n2
        elif o==0xD:nx=((n1<<4)|n2) if C else PC+1
        elif o==0xE:nx=((n1<<4)|n2) if Z else PC+1
        elif o==0xF:
            if w==0xF07:nx=RA
            elif w==0xF08:R=[0]*16
            elif w==0xF03: # HXDZ
                num=R[0xD]+16*R[0xE]+256*R[0xF]
                if num>999: Z=1;R[0xD]=R[0xE]=R[0xF]=0
                else: Z=0;R[0xD]=num%10;R[0xE]=(num//10)%10;R[0xF]=(num//100)%10
                C=0
            elif w==0xF04: # DZHX
                num=R[0xD]+10*R[0xE]+100*R[0xF]
                R[0xD]=num%16;R[0xE]=(num//16)%16;R[0xF]=(num//256)%16;C=0;Z=0
            elif w==0xF0B: # MULT
                num=bcd(R[0:6])*bcd(EX[0:6]); C=1 if num>999999 else 0
                num%=1000000
                for i in range(6): R[i]=(num//(10**i))%10
            elif w==0xF0C: # DIV  quotient=EX/ R -> R; rem -> EX
                d=bcd(R[0:4]); n=bcd(EX[0:4])
                if d==0: C=1
                else:
                    C=0;q=n//d;r=n%d
                    for i in range(4): R[i]=(q//(10**i))%10
                    for i in range(4): EX[i]=(r//(10**i))%10
            elif w==0xF0D: # EXRL swap R0-7 <-> EX0-7
                for i in range(8): R[i],EX[i]=EX[i],R[i]
            elif w==0xF0E:
                for i in range(8,16): R[i],EX[i]=EX[i],R[i]
            elif n1==0xF:
                if ki<len(keys): R[n2]=keys[ki];ki+=1
                else: seq.append('WAIT');break
        PC=nx&0xFF
    return seq
prog=sys.argv[1]
P = load_lst(prog) if prog.endswith('.lst') else load_mic(prog)
for label,keys in [('human cell1,5,3',[1,5,3]),('human cell2,4,8',[2,4,8]),('comp-first then 1,5',[0,1,5])]:
    print('%-22s'%label, run(P,keys))
