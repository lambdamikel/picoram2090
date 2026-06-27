PicoRAM Merlin
===============

A graphical 3x3 Merlin / Lights-Out style puzzle (titled "MERLIN" on the OLED) for the Busch Microtronic 2090 with PicoRAM.

Load PICO_WIPEON_SND.MIC and start at address 00.
Set PicoRAM to SOUND mode and reset PicoRAM if you want the key tones and victory melody.
The game still works with audio muted or in TTS mode, but the 50D sound opcodes will not play notes.

Goal
----
Turn all nine cells ON. The OLED shows O for on and . for off.

Controls
--------
The keypad's left 3x3 block maps to the on-screen board - the physical
Microtronic key rows line up with the display rows:

  keys        board cells
  8 9 A   ->  1 2 3
  4 5 6   ->  4 5 6
  0 1 2   ->  7 8 9

Press a key to apply that cell's Merlin-style region toggle.
F: new random puzzle.
Any key after the victory melody: new random puzzle.

Each key press gives a short confirmation beep. The victory melody now
plays ~19x faster (it was extremely slow). The board is overwritten each
move instead of cleared+redrawn, so it no longer blinks (the OLED is
cleared once at the start of each game).

Cell layout
-----------
1 2 3
4 5 6
7 8 9

Toggle rules
------------
1 -> 1245
2 -> 123
3 -> 2356
4 -> 147
5 -> 24568
6 -> 369
7 -> 4578
8 -> 789
9 -> 5689

Implementation notes
--------------------
Board rows are stored as 3-bit values in R0-R2: left=1, middle=2, right=4.
RND generates three row values, masked to 0..7. The 9 toggle masks are full rank, so every 3x3 pattern is solvable.
The program uses existing PicoRAM extended opcodes only: 502, 504, 506, 508, 50D.
No PicoRAM firmware changes are required.
The program is one bank only. Last used address: E6 (APPLY2 short-beep routine at DB-E6).
CALL/RET is not nested: DRAWROW and DELAY are leaf subroutines only. The key beep (APPLY2) is reached by GOTO and ends in GOTO - no CALL.
