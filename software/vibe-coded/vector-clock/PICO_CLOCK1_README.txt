VECTOR CLOCK  -  Busch Microtronic 2090 + PicoRAM
=================================================

Load PICO_CLOCK1.MIC and start at address 00. Uses the PicoRAM's
battery-buffered real-time clock and the OLED. (No TTS needed - this one is
display-only. Spoken time may come later.)

The idea
--------
A big-digit wall clock. The Microtronic's red LED shows the live time
HH MM SS (ticking from the RTC); when it reads correctly, you press a key and
the OLED draws HH:MM as four large **vector 7-segment digits** with a colon -
each segment a real drawn line, not a text character. Press again to refresh.

Using it
--------
  1. On start, the red LED shows HH MM SS from the RTC (F6A/F06).
  2. Press any key -> the OLED clears and draws the current HH:MM big.
  3. The drawn clock stays on screen. Press again whenever you want to update
     it; the LED keeps showing the live time in between.

So it's a "confirmed read" clock: you only ever draw a time you have just
seen on the LED, which side-steps the async RTC-load timing entirely (below).

The display
-----------
  Red LED  = live HH MM SS straight from the RTC (F06 reload + F6A show)
  OLED     = HH : MM as four 7-segment digits (~11 x 23 px each) at
             x = 0 / 32 / 64 / 96, colon at x = 53, drawn with 50A lines and
             the x-high nibble injected per digit via 3F0.

Digits share segment tails to save space (8 = G+0, 6 = E+5, 9 = F+3,
7 = A+1). Registers: R1=min1 R2=min10 R3=hour1 R4=hour10 (captured from the
RTC registers RC-RF), R0=digit x-high, R8=position, R9=digit value.

The interesting part - beating the F06 RTC-load race
----------------------------------------------------
On the PicoRAM, F06 ("load the RTC") is not a normal instruction. The Pico
intercepts it and runs a hidden **asynchronous detour**: it rewrites RAM under
the running CPU to materialize MOVI words that load the six BCD time digits
(RA=sec1 RB=sec10 RC=min1 RD=min10 RE=hour1 RF=hour10), then jumps back to
F06+1 - and disarms the detour on the next address change. That disarm is a
race: if it slips by one fetch, the instruction right after F06 (or anything in
the detour's fixed 0x20-0x30 scratch zone) gets served from the wrong array,
so a KIN can be skipped or a clear can fire early.

This program dodges the race by construction, in one bank:

  * F06 sits at 02, and 0x03-0x30 is a solid **NOP runway** (000 = MOV R0,R0,
    a CPU no-op). However the detour/disarm lands, it can only hit no-ops.
  * The KIN wait is at **0x31** - past the whole detour zone - so it is always
    served clean and actually blocks. The screen-clear (502) sits *behind* the
    KIN, so it can never run before a real keypress.
  * The loop-back is **GOTO 01, never GOTO 00** - the PicoRAM can't serve a jump
    to address 00, so 00 is the power-on entry only and the loop re-enters at 01.

The result is rock-solid: the clock draws exactly when you press a key, holds,
and never flickers or self-clears. One bank, occupies 00-E7 (a good chunk of
which is the deliberate NOP runway). Uses only existing PicoRAM op-codes
(F06/F6A, 502, 50A, 3F0, FF0) - no firmware changes. CALL/RET never nested.
Assembly source included (clock1.asm; assemble with dev-support/masm.py).

Written (vibe-coded) by Claude (Anthropic, Opus 4.8), directed by Michael Wessel.
