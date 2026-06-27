import sys,re
def load_mic(p):
    prog={};a=0;st=False
    for l in open(p).read().splitlines():
        s=l.strip()
        if s.startswith('@'): a=int(s.split()[1],16);st=True;continue
        if not st or s=='' or s.startswith('#'): continue
        prog[a]=int(s,16);a+=1
    return prog
def bcd(rs): return sum((rs[i]&0xF)*(10**i) for i in range(len(rs)))
W,H=128,32
def line(fb,x1,y1,x2,y2):
    dx=abs(x2-x1);dy=-abs(y2-y1);sx=1 if x1<x2 else -1;sy=1 if y1<y2 else -1;e=dx+dy
    while True:
        if 0<=x1<W and 0<=y1<H: fb[y1][x1]='#'
        if x1==x2 and y1==y2: break
        e2=2*e
        if e2>=dy:e+=dy;x1+=sx
        if e2<=dx:e+=dx;y1+=sy
def run(prog,keys,maxsteps=80000):
    R=[0]*16;EX=[0]*16;PC=0;RA=0;Z=0;C=0;ki=0
    fb=[[' ']*W for _ in range(H)]; tgrid=[[' ']*16 for _ in range(4)]
    mode=None;am=0;a=[0,0,0,0];cx=cy=0; moves=[]; spoke=[]; des=[]
    for st in range(maxsteps):
        if PC not in prog: moves.append('FELL@%02X'%PC);break
        w=prog[PC];o=w>>8;n1=(w>>4)&0xF;n2=w&0xF;nx=PC+1
        # ---- PicoRAM bus ----
        if o==5 and n1==0:
            am=0
            if n2==2: fb=[[' ']*W for _ in range(H)]; tgrid=[[' ']*16 for _ in range(4)]
            elif n2==0xA: mode='LINE';a=[0,0,0,0]
            elif n2==8: mode='CUR'
            elif n2==6: mode='CHR'
            elif n2==0xF: mode='SPK'
            elif n2==4: pass
        elif o==0 and n1==n2:    # 0xx data word
            v=n2
            if mode=='LINE':
                idx=am//2;hi=am%2
                if hi==0:a[idx]=v
                else:
                    a[idx]+=16*v
                    if idx==3: line(fb,a[0]%128,a[1]%32,a[2]%128,a[3]%32);am=-1
                am+=1
            elif mode=='CUR':
                if am==0:cx=v%16;am=1
                else:cy=v%4;am=0
            elif mode=='CHR':
                if am==0:a[0]=v;am=1
                else:
                    a[0]+=16*v;ch=chr(a[0]) if 32<=a[0]<127 else '?'
                    if 0<=cy<4 and 0<=cx<16: tgrid[cy][cx]=ch
                    cx+=1;am=0
            elif mode=='SPK':
                if am==0:a[0]=v;am=1
                else:
                    a[0]+=16*v
                    if a[0]==10: spoke.append(''.join(getattr(run,'_b',[])) ); run._b=[]
                    elif 32<=a[0]<127: getattr(run,'_b',run.__dict__.setdefault('_b',[])).append(chr(a[0]))
                    am=0
            else: des.append((PC,w))
        # ---- CPU ----
        if o==0:R[n2]=R[n1]
        elif o==1:R[n2]=n1
        elif o==2:R[n2]&=R[n1]
        elif o==3:R[n2]&=n1
        elif o==4:vv=R[n2]+R[n1];C=1 if vv>15 else 0;R[n2]=vv&15
        elif o==5:
            if n1:vv=R[n2]+n1;C=1 if vv>15 else 0;R[n2]=vv&15
        elif o==6:vv=R[n2]-R[n1];C=1 if vv<0 else 0;R[n2]=vv&15
        elif o==7:vv=R[n2]-n1;C=1 if vv<0 else 0;R[n2]=vv&15
        elif o==8:Z=1 if R[n2]==R[n1] else 0;C=1 if R[n2]<R[n1] else 0
        elif o==9:Z=1 if R[n2]==n1 else 0;C=1 if R[n2]<n1 else 0
        elif o==0xA:R[n2]|=R[n1]
        elif o==0xB:RA=PC+1;nx=(n1<<4)|n2
        elif o==0xC:nx=(n1<<4)|n2
        elif o==0xD:nx=((n1<<4)|n2) if C else PC+1
        elif o==0xE:nx=((n1<<4)|n2) if Z else PC+1
        elif o==0xF:
            if w==0xF07:nx=RA
            elif w==0xF08:R=[0]*16
            elif w==0xF03:
                num=R[0xD]+16*R[0xE]+256*R[0xF]
                if num>999:Z=1;R[0xD]=R[0xE]=R[0xF]=0
                else:Z=0;R[0xD]=num%10;R[0xE]=(num//10)%10;R[0xF]=(num//100)%10
            elif w==0xF0B:
                num=bcd(R[0:6])*bcd(EX[0:6]);num%=1000000
                for i in range(6):R[i]=(num//(10**i))%10
            elif w==0xF0C:
                d=bcd(R[0:4]);n=bcd(EX[0:4])
                if d:
                    q=n//d;r=n%d
                    for i in range(4):R[i]=(q//(10**i))%10
                    for i in range(4):EX[i]=(r//(10**i))%10
            elif w==0xF0D:
                for i in range(8):R[i],EX[i]=EX[i],R[i]
            elif n1==0xF:
                if ki<len(keys):R[n2]=keys[ki];ki+=1
                else: moves.append('WAIT');break
        PC=nx&0xFF
    return fb,tgrid,moves,spoke,des
prog=load_mic(sys.argv[1]); keys=[int(x,16) for x in sys.argv[2:]]
fb,tg,mv,sp,des=run(prog,keys)
print('result:',mv[-1] if mv else '?','  spoke:',sp,'  desync:',des or 'none')
print('+'+'-'*64+'+')
for y in range(16):
    row=''
    for x in range(64):
        c=fb[y*2][x*2] if fb[y*2][x*2]!=' ' else fb[y*2][x*2+1]
        row+=c
    # overlay text grid (16 cols x 4 rows -> map to 64x16)
    print('|'+row+'|')
print('+'+'-'*64+'+')
print('text grid:')
for r in tg: print('  |'+''.join(r)+'|')
