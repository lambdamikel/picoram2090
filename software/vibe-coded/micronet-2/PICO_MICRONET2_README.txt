MICRONET 2  -  Busch Microtronic 2090 + PicoRAM
===============================================

Load PICO_MICRONET2.MIC and start at address 00. Set the PicoRAM's TTS/SOUND
switch to TTS to hear it speak (with the switch on SOUND it runs silently and
still shows everything).

The idea
--------
The classic MICRONET demo - the diagonal net drawn line by line on the OLED,
then "MICROTRONIC" in the top (yellow) band, spoken by the TTS - but
self-restarting: the original ends in F00 (program stop) and has to be
restarted by hand. MICRONET 2 instead admires its finished screen for a few
seconds, wipes the display, and draws the whole thing again. Forever. Perfect
for an exhibit table.

One cycle (about 10 seconds)
----------------------------
  1. wipe the OLED
  2. draw the net: 16 lines, animated - every top anchor (x = 0/32/64/96 at
     y = 8) connected to every bottom anchor (same x positions at y = 31),
     exactly the original's geometry (50A lines with the X coordinates
     injected from R0/R1 via 3F0/3F1)
  3. print M-I-C-R-O-T-R-O-N-I-C at the top and SPEAK it (50E speak-echo:
     the printed characters double as the speech buffer, the newline says it)
  4. hold for ~5 seconds, then start over

The LED display and the DOT lights
----------------------------------
The Microtronic's own 6-digit LED display is on the whole time (F60 = show all
six digits from R0):

  - digits 1-2 : R0, R1, the net's top/bottom anchor indices (they step 0->8
                 as the net is woven, so these two churn while it draws)
  - digits 3-6 : R2:R3:R4:R5, a 4-digit counter that ticks up once per line
                 drawn - a running "lines woven so far" total across all cycles

The four DOT output LEDs blink the whole time too:
  - while drawing : DOT 5 shows the line counter's low nibble, so the LEDs run
                    a little binary count as each net line appears
  - during the pause : DOT F flickers the LEDs with the pause countdown

(If you'd rather the LEDs track a net coordinate instead of the counter, change
DOT 5 at label BLINK to DOT 0 - then they show the top anchor index R0.)

Tuning
------
The pause length is one word: the MOVI at label PAUSE (see micronet2.asm).
MOVI 4,E is ~5 s; each count is ~1.2 s. The warm-up delay at the start
(MOVI 2,E, ~2.5 s) gives the OLED time to come up after a cold start.

Register map
------------
  R0/R1   net anchors (x = 16*R)      R2:R3:R4:R5   line counter (shown + on DOT)
  RE/RF   delay counters              R6-RD         unused

Files
-----
  PICO_MICRONET2.MIC   the program (84 words, one bank)
  micronet2.asm        assembler source (masm.py, see dev-support)

Verified: the net drawing (unchanged from the hardware-tested original) plus
the new counter / DOT / 6-digit-display logic, run in the Microtronic simulator
- runs forever, the counter advances with correct carry, the animation loop
never stalls. The line graphics themselves are drawn by the PicoRAM on the real
hardware.

Written (vibe-coded) by Claude (Anthropic), directed by Michael Wessel.
