# Dev Support - tools used to build the vibe-coded programs

Small Python tools written along the way. They need no dependencies (plain
Python 3) and don't touch the PicoRAM firmware - they just let you assemble and
*simulate* Microtronic + PicoRAM programs off-device.

## `masm.py` - Microtronic assembler

Turns a readable `.asm` (labels -> addresses, mnemonics, and raw `xxx` data
words) into a `.MIC` file ready to load onto the PicoRAM SD card.

```
python3 masm.py source.asm out.MIC
```

Mnemonics: `MOV/MOVI/AND/ANDI/ADD/ADDI/SUB/SUBI/CMP/CMPI/OR`, `CALL/GOTO/BRC/BRZ`,
`RET/CLEAR/HXDZ/MULT/DIV/EXRL/...`, `INV/SHR/SHL/DOT/KIN/...`, `DISP x,y`, `.ORG xx`,
and any bare 3-hex word as a literal (handy for the `50x` graphics/sound op-codes
and `70x` bank switches). The `.asm` sources of the Tic-Tac-Toe games were written
for this assembler - see the program folders.

> Gotcha baked in: `ADD` / `ADC` are *also* valid 3-hex strings, so mnemonics are
> matched before the raw-literal fallback. (Learned the hard way.)

## `mtsim.py` - two-bank Microtronic + PicoRAM simulator

The headline tool. Runs a program (one **or two** banks) and renders the OLED as
text plus a transcript of the TTS speech, so an entire game can be played and
verified without hardware.

```
python3 mtsim.py BANK0.MIC [BANK1.MIC] --keys 8,5,9      # feed hex keys at KIN
```

It models the hardware quirks that actually mattered:

* **No-borrow carry** - `CMPI n,r` sets carry when `R[r] > n` (and `CMP` when
  `R[s] < R[d]`), the *opposite* of a naive borrow; `ADD` carry is overflow.
* **Bank switch `70x`** - can't move the PC (it lands at +1) and has a
  one-instruction **prefetch** (the +1 word still runs from the old bank), so a
  reliable handoff is `70x ; NOP ; GOTO target`.
* **`50x` ext-op argument decoding** - the equal-nibble `0xx` words (`MOV x,x`
  register no-ops) are consumed as `502/504/506/508/50A/50F` arguments.

It does **not** model ext-op latency: on real hardware the draw/speech ops are
buffered on the Pico's 2nd core, so leave a short wait loop before any bank
switch that follows a burst of `50x` ops, or the Pico can miss the `70x`.

## Older / single-purpose simulators

These came first and are more specific, kept for reference:

* `gamesim.py` - single-bank CPU + BCD math + OLED renderer + `50x` arg decoder
  (used to verify the computer-first Tic-Tac-Toe against the manual's worked
  example).
* `overlay.py` - wraps `gamesim` to draw a 3x3 grid/cell overlay.
* `fullsim.py` - bare CPU emulator with full BCD ops (`HXDZ/MULT/DIV`), used to
  diagnose the original (corrupt) GPT tic-tac-toe.
* `mtsim2.py` - bus-accurate decoder that flags `50x` argument-stream **desyncs**
  and `3Fx` CPU-hijacks (the "stray `0xx` word gets swallowed as an arg" trap).
