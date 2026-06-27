PicoRAM graphical BLOCKADE  (Busch Microtronic 2094 "Computerspiele")
=====================================================================

Load PICO_BLOCKADE_GFX.MIC and start at address 00. Set PicoRAM to TTS mode for
the "YOU WIN" / "I WIN" speech (the game also plays fine silently in any mode).

The game
--------
BLOCKADE is the two-track strategy game from the Busch 2094 "Computerspiele"
booklet (printed page 7). There are two lanes:

  Bahn 1:  fields  1 2 3 4 5 6 7 8 9 A   (10 fields)
  Bahn 2:  fields  1 2 3 4 5             (5 fields)

Each lane has a computer piece (O) and your piece (X). The computer starts at the
left end of each lane, you start at the right end; every turn one piece is moved
toward the other, closing the gap.

  O = computer,   X = you.

OLED display
------------
The OLED shows each lane as its row of field numbers, with the pieces sitting
right under their field, e.g. at the start:

   123456789A      <- Bahn 1: computer O on field 1, your X on field 10
   O        X
   12345           <- Bahn 2: O on 1, X on 5
   O   X

So you can read the target field directly and watch both gaps close as you play.
(The original red 7-segment LED display keeps working as well.)

Controls (a move is TWO key presses)
------------------------------------
  first key:   the lane number   - 1 or 2
  second key:  the target field  - lane 1: 1..A,  lane 2: 1..5

Example: "1 5" moves your piece on lane 1 to field 5. The computer then plays its
reply (its O moves) and the board redraws.

Winning
-------
Make the number of empty fields between the pieces the SAME on BOTH lanes (the
manual's "Gewinnstrategie"). It is a Nim-style game: the first player (you) can
always win with correct play - play loosely and the computer wins. From the start
(8 empty on lane 1, 3 on lane 2) the winning first move is "1 5" (3 empty on each).

  You win       -> speaks "YOU WIN",  LED shows CCC.
  Computer wins -> speaks "I WIN",    LED shows FFF.
Press any key to start a new game.

What this is / what was done
----------------------------
The game logic is the AUTHENTIC Busch program (addresses 00-87), transcribed and
verified word-for-word against anl2094.pdf - not a re-implementation. (The earlier
GPT "graphical" version was broken; its 00-87 core was in fact correct, but the
OLED wrapper around it was not.) On top of the authentic program, only three
hooks were added:

  06: CALL DRAWPIECES - update the OLED (see below).
  41: -> CWIN          route the computer-win branch through an "I WIN"  speech.
  6D: -> PWIN          route the player-win branch through a "YOU WIN" speech.

Fast incremental redraw: the static lane numbers are drawn only ONCE (the first
time DRAWPIECES runs, gated on a flag register). After that, every turn only the
four pieces are erased and redrawn at their columns - no full-screen 502 clear and
no number redraw - so play is quick and flicker-free instead of redrawing the
whole screen each move. The previous piece columns are remembered (RA/RB/RC/RD) so
the old O/X can be blanked; positions are fed to the cursor via the PicoRAM 3Fx op
(a register as the cursor-x argument). The win helpers redraw the final board
before speaking, so you see the finishing position.

Implementation notes
--------------------
One bank, 00-FF (256 words, full). Authentic game 00-87; DRAWPIECES 88-DD (R9 =
"numbers drawn" flag, R8 = number-loop counter, RA/RB/RC/RD = previous piece
columns); CWIN DE-EC; PWIN ED-FF. Uses only existing extended op-codes
(502/504/506/508/50F and 3Fx). CALL/RET is never nested (DRAWPIECES and the win
helpers are leaf routines). No PicoRAM firmware changes required.
