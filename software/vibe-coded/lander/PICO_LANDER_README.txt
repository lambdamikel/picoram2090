LUNAR LANDER  -  Busch Microtronic 2090 + PicoRAM
==================================================

Load PICO_LANDER.MIC and start at address 00. Set PicoRAM to TTS mode for the
touchdown call-out.

The idea
--------
Bring the lander down to the surface gently. Gravity pulls you down a little
faster every turn; your thruster slows you but burns fuel, and you only have so
much. Touch down at a velocity of 2 or less and you have landed - come in faster
and you crash.

It is turn-based, which is the *original* 1969 Lunar Lander (each turn you pick a
burn and the numbers advance one step) - and a perfect fit for a machine that
politely waits for a keypress instead of running in real time.

The display - a little cockpit
------------------------------
  Red LED  = the instruments:  ALT  VEL  FUEL   (altitude / velocity / fuel)
  OLED     = the view out the window: your lander descending toward the ground,
             with a thrust flame beneath it when you burn, and a smoking crater
             if you come in too hard.

Each turn the LED shows ALT VEL FUEL and the OLED shows where you are above the
surface.  (If the three LED digits read in a different order than ALT VEL FUEL,
let me know and I'll swap them - it's a one-line change.)

Playing
-------
Each turn, press ONE key = how hard to fire the thruster this turn (0-F):
  0       = no burn  (free fall - you speed up)
  about 1 = roughly cancels gravity  (hover / gentle descent)
  higher  = brake hard (slows or stops your fall), but spends that much fuel

You start at altitude 12, velocity 1, fuel 15.
  Touch down with VEL <= 2  ->  "THE EAGLE HAS LANDED"
  Touch down with VEL > 2   ->  "CRASH"
Press any key to fly again.

Strategy: don't free-fall (you'll build up too much speed to stop), and don't
panic-burn early (you'll run the tank dry with nothing left to brake with). Let
it fall a while, then brake as the ground rushes up.

Implementation notes
--------------------
One bank, 172 words (00-AA). R1=ALT, R2=VEL, R3=FUEL, R0=thrust. The physics is
pure integer: VEL += gravity - thrust; ALT -= VEL; FUEL -= thrust; over-thrust
just hovers (VEL clamped at 0). The screen is cleared and the ground line drawn
once at the start of every game; during play only the lander glyph and its thrust
flame move (erase old, draw new) - the fast/flicker-free trick. A clean touchdown
leaves the lander on the pad; a crash stamps a crater (*X*). Uses only existing
extended op-codes (502/504/506/508/50A/50F and 3Fx). CALL/RET is never nested. No
PicoRAM firmware changes. Assembly source included (lander.asm; assemble with
masm.py).

The lander is a single glyph ('A') and the flame a 'V' - easy to change for a
different look.

Written by Claude (Anthropic, Opus 4.8) - my own first Microtronic program, and
the first in the collection that's an original design rather than a port. :-)
