PicoRAM MERLIN 2  (Merlin / Lights-Out 3x3, with difficulty + move budget)
==========================================================================

Load PICO_MERLIN2_SND.MIC and start at address 00. Set PicoRAM to SOUND mode
for the key beep and the melodies.

This is the second version of MERLIN. It adds a selectable move budget
(difficulty) and a "you ran out of moves" loss, in addition to the move
counter (shown on the red 7-segment LED display).

Start of a game
---------------
The OLED clears and the program waits for ONE key: that key is your
MOVE BUDGET (difficulty).

  press   budget (moves)   feel
  8..F    8..15            fair to easy   <- recommended
  1..7    1..7             hard / expert

Lower budget = fewer moves allowed = harder. (Avoid 0.)
Then the board appears and you play.

Controls (same keypad-to-grid layout as MERLIN)
-----------------------------------------------
  keys        board cells
  8 9 A   ->  1 2 3   (top)
  4 5 6   ->  4 5 6   (middle)
  0 1 2   ->  7 8 9   (bottom)

Press a cell key to apply its Merlin region toggle. Each press gives a short
beep and bumps the move counter on the LED.
F = abandon and start a new game (re-selects difficulty).

Goal / win / lose
-----------------
Turn all nine cells ON (O = on, . = off) before your moves run out.
- Solve it  -> victory melody (rising arpeggio).
- LED move count reaches your budget without solving -> SAD melody
  (falling D-minor), then a new game starts.

What changed vs MERLIN v1
-------------------------
- Difficulty / move-budget selection at start; loss on budget exceeded.
- Both melodies sped up ~19x (the old DELAY was ~18 s per note!).
- The board is now OVERWRITTEN each move instead of cleared+redrawn, so it
  no longer blinks. The OLED is cleared once at the start of each game.

Implementation notes
--------------------
One 256-word bank (00-FF, full). RC = move budget, R4 = move counter.
Added routines: DIFF E7-EB, LOSECHK EC-EE, sad melody EF-FF.
No PicoRAM firmware changes; CALL/RET is never nested.
