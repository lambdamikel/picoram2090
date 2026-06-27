import sys
exec(open('/tmp/gamesim.py').read().split("prog=load_mic(sys.argv[1])")[0])
prog=load_mic(sys.argv[1]); keys=[int(x,16) for x in sys.argv[2:]]
fb,tg,mv,sp,des=run(prog,keys)
print('result:',mv[-1] if mv else '?',' spoke:',sp,' desync:',des or 'none')
# 128x32 fb downscaled x/2,y/2 -> 64x16 ; overlay text cells at (col*8,row*8)->(/2)
out=[[fb[y*2][x*2] if fb[y*2][x*2]!=' ' else fb[y*2][x*2+1] for x in range(64)] for y in range(16)]
for r in range(4):
    for c in range(16):
        ch=tg[r][c]
        if ch!=' ':
            px=(c*8)//2; py=(r*8)//2
            if 0<=py<16 and 0<=px<64: out[py][px]=ch
print('+'+'-'*64+'+')
for row in out: print('|'+''.join(row)+'|')
print('+'+'-'*64+'+')
