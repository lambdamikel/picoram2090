#!/usr/bin/env python3
"""
mtsim.py - a small Busch Microtronic 2090 + PicoRAM simulator, with BANK SWITCHING.

Written (vibe-coded) while building the two-bank human-first Tic-Tac-Toe. It models
the few hardware quirks that actually bit us, so a program can be checked off-device:

  * CPU core: MOV/MOVI/AND/ANDI/ADD/ADDI/SUB/SUBI/CMP/CMPI/OR, CALL/RET (single level,
    no stack), GOTO/BRC/BRZ, CLEAR, KIN, plus the F-ops we needed.
  * CARRY after compare/subtract is NO-BORROW: `CMPI n,r` sets carry when R[r] > n
    (and `CMP s,d` when R[s] < R[d]) - the opposite of a naive borrow. ADD/ADDI carry
    is overflow (>15), as expected. (This one silently cost us a debugging round.)
  * BANK SWITCH `70x` (op 7, mid-nibble 0): switches the active SRAM bank to x. It is
    physically a `SUBI 0,x` (a register no-op). It CANNOT move the PC - after it the PC
    is just +1 - AND there is a one-instruction PREFETCH: the word at +1 still executes
    from the OLD bank; the new bank only takes over at +2. So a reliable handoff is
    `70x ; NOP ; GOTO target`.
  * PicoRAM extended op-codes are decoded for display/speech: 502 clear, 504 refresh,
    506 char(lo,hi), 508 cursor(x,y), 50A line(x1,y1,x2,y2), 50F speak byte. Their
    arguments are the *equal-nibble* words 0xx (000,011,...,0FF = `MOV x,x`, value x),
    which are register no-ops on the CPU but consumed as ext-op args by the PicoRAM.
  * KIN feeds keys from --keys (hex), in order.

NOTE: this does NOT model ext-op latency/async (the buffered draw/speech on the Pico's
2nd core). On real hardware, give the Pico time (a short wait loop) before a bank switch
that follows a burst of 50x ops, or it may miss the 70x.

Usage:
  python3 mtsim.py BANK0.MIC [BANK1.MIC] [--keys 8,5,9] [--max 300000] [--trace]
"""
import sys, argparse

def load_mic(fn):
    prog = {}
    addr = 0
    started = False
    for line in open(fn).read().splitlines():
        s = line.strip()
        if s.startswith('@'):
            addr = int(s.split()[1], 16); started = True; continue
        if not started or s == '' or s.startswith('#'):
            continue
        prog[addr] = int(s, 16); addr += 1
    return prog

LINES = [(1,2,3),(4,5,6),(7,8,9),(1,4,7),(2,5,8),(3,6,9),(1,5,9),(3,5,7)]  # for convenience

class Sim:
    def __init__(self, banks, keys, trace=False):
        self.banks = banks                 # {bank_index: {addr: word}}
        self.keys = list(keys)
        self.trace = trace
        self.R = [0]*16
        self.PC = 0
        self.Z = 0; self.C = 0
        self.ret = None
        self.cur = 0
        self.pend = None                   # deferred bank switch (1-instruction prefetch)
        self.ki = 0
        self.chars = {}                    # (text_x, text_y) -> char  (from 506 at 508 cursor)
        self.lines = []                    # (x1,y1,x2,y2)             (from 50A)
        self.speech = []                   # spoken strings           (from 50F + newline)
        self.mode = None; self.argbuf = []; self.spk = []
        self.cursor = (0, 0)

    def render_oled(self):
        grid = [['.' for _ in range(16)] for _ in range(4)]
        for (cx, cy), ch in self.chars.items():
            if 0 <= cx < 16 and 0 <= cy < 4:
                grid[cy][cx] = ch
        out = ['  +' + '-'*16 + '+']
        for r in range(4):
            out.append('  |' + ''.join(grid[r]) + '|')
        out.append('  +' + '-'*16 + '+')
        if self.lines:
            out.append('  lines: ' + ', '.join('(%d,%d)-(%d,%d)' % L for L in self.lines))
        return '\n'.join(out)

    def step(self):
        prog = self.banks[self.cur]
        w = prog.get(self.PC, 0)
        o = w >> 8; n1 = (w >> 4) & 0xF; n2 = w & 0xF
        nxt = self.PC + 1
        apply_pending_after = True

        # ---- PicoRAM 50x ext-ops ----
        if o == 5 and n1 == 0:
            if   n2 == 0x2: self.chars.clear(); self.lines.clear(); self.mode=None; self.argbuf=[]
            elif n2 == 0x4: self.mode=None
            elif n2 == 0xA: self.mode='LINE'; self.argbuf=[]
            elif n2 == 0x8: self.mode='CUR';  self.argbuf=[]
            elif n2 == 0x6: self.mode='CHAR'; self.argbuf=[]
            elif n2 == 0xF: self.mode='SPK';  self.argbuf=[]; self.spk=[]
            else: self.mode=None
            self.PC = nxt & 0xFF
            if self.pend is not None: self.cur=self.pend; self.pend=None
            return True
        # ---- equal-nibble 0xx word = ext-op argument (also MOV x,x no-op) ----
        if o == 0 and n1 == n2 and self.mode is not None:
            self.argbuf.append(n1)
            need = {'LINE':8,'CUR':2,'CHAR':2,'SPK':2}[self.mode]
            if len(self.argbuf) == need:
                a = self.argbuf
                if self.mode == 'LINE':
                    self.lines.append((a[0]+16*a[1], a[2]+16*a[3], a[4]+16*a[5], a[6]+16*a[7]))
                elif self.mode == 'CUR':
                    self.cursor = (a[0], a[1])
                elif self.mode == 'CHAR':
                    self.chars[self.cursor] = chr(a[0] + 16*a[1])
                elif self.mode == 'SPK':
                    asc = a[0] + 16*a[1]
                    if asc == 10: self.speech.append(''.join(self.spk)); self.spk=[]
                    else: self.spk.append(chr(asc))
                self.argbuf = []
            self.PC = nxt & 0xFF
            if self.pend is not None: self.cur=self.pend; self.pend=None
            return True
        # ---- bank switch 70x : deferred by one instruction (prefetch) ----
        if o == 7 and n1 == 0:
            self.pend = n2
            self.PC = nxt & 0xFF
            return True                      # do NOT apply pend after the 70x itself
        # ---- CPU core ----
        if   w == 0xF07:                      # RET
            nxt = self.ret if self.ret is not None else 0; self.ret = None
        elif o == 0xB: self.ret = self.PC+1; nxt = (n1<<4)|n2          # CALL
        elif o == 0xC: nxt = (n1<<4)|n2                                # GOTO
        elif o == 0xD: nxt = ((n1<<4)|n2) if self.C else self.PC+1     # BRC
        elif o == 0xE: nxt = ((n1<<4)|n2) if self.Z else self.PC+1     # BRZ
        elif w == 0xF08: self.R = [0]*16                              # CLEAR
        elif o == 0xF and n1 == 0xF:                                  # KIN x
            if self.ki >= len(self.keys):
                if self.pend is not None: self.cur=self.pend; self.pend=None
                return 'KIN'                  # out of keys: stop, caller may render
            self.R[n2] = self.keys[self.ki] & 0xF; self.ki += 1
        elif o == 0: self.R[n2] = self.R[n1]
        elif o == 1: self.R[n2] = n1
        elif o == 2: self.R[n2] &= self.R[n1]
        elif o == 3: self.R[n2] &= n1
        elif o == 4: v=self.R[n2]+self.R[n1]; self.C=1 if v>15 else 0; self.R[n2]=v&15
        elif o == 5: v=self.R[n2]+n1;         self.C=1 if v>15 else 0; self.R[n2]=v&15
        elif o == 6: v=self.R[n2]-self.R[n1]; self.C=1 if v<0  else 0; self.R[n2]=v&15
        elif o == 7: v=self.R[n2]-n1;         self.C=1 if v<0  else 0; self.R[n2]=v&15
        elif o == 8: self.Z=1 if self.R[n2]==self.R[n1] else 0; self.C=1 if self.R[n1]<self.R[n2] else 0  # CMP: no-borrow
        elif o == 9: self.Z=1 if self.R[n2]==n1 else 0;         self.C=1 if self.R[n2]>n1 else 0          # CMPI: no-borrow
        elif o == 0xA: self.R[n2] |= self.R[n1]
        # (other F-ops: treated as no-ops here)
        self.PC = nxt & 0xFF
        if apply_pending_after and self.pend is not None:
            self.cur = self.pend; self.pend = None
        return True

    def run(self, maxsteps):
        for _ in range(maxsteps):
            r = self.step()
            if r == 'KIN':
                return 'KIN'
            if self.trace:
                print('  %d:%02X  %03X' % (self.cur, self.PC, self.banks[self.cur].get(self.PC,0)))
        return 'MAX'

def main():
    ap = argparse.ArgumentParser(description='Two-bank Microtronic + PicoRAM simulator')
    ap.add_argument('banks', nargs='+', help='BANK0.MIC [BANK1.MIC ...]')
    ap.add_argument('--keys', default='', help='hex keys to feed at KIN, comma/space separated, e.g. 8,5,9')
    ap.add_argument('--max', type=int, default=300000)
    ap.add_argument('--trace', action='store_true')
    a = ap.parse_args()
    banks = {i: load_mic(fn) for i, fn in enumerate(a.banks)}
    keys = [int(k, 16) for k in a.keys.replace(',', ' ').split()] if a.keys else []
    sim = Sim(banks, keys, a.trace)
    res = sim.run(a.max)
    print('stopped: %s   (at bank %d, PC %02X, after %d keys)' % (res, sim.cur, sim.PC, sim.ki))
    print('OLED:')
    print(sim.render_oled())
    print('registers:', ' '.join('%X' % r for r in sim.R))
    if sim.speech:
        print('speech:', ' / '.join(sim.speech))

if __name__ == '__main__':
    main()
