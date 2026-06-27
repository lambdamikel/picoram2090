PicoRAM Tic-Tac-Toe  -  computer-first, graphical
=================================================

Load PICO_TITATO_CF.MIC and start at address 00. Set PicoRAM to TTS mode for
the spoken prompts (the DOT/buzzer win signal works in any mode).

A clean graphical build wrapped around the ORIGINAL Busch tic-tac-toe strategy
(Manual Part 2, printed page 4 - the 58-word "Tic-Tac-Toc" program), preserved
byte-for-byte. The earlier graphical version was a corrupted transcription;
this one is not.

How to play
-----------
The computer opens in the center. The keypad maps directly to the board:

  keys        board cell
  8 9 A   ->  top    row (left, middle, right)
  4 5 6   ->  middle row
  0 1 2   ->  bottom row

Press the key for the cell you want. Your mark (X) and the computer's reply (O)
are drawn inside a full 3x3 grid on the 128x32 OLED. On your turn it asks
"YOU?". It plays the original Busch look-ahead strategy and does not lose - a
win flashes the DOT/buzzer and says "I WIN"; a full board says "DRAW". Press
any key to start a new game.

Notes
-----
247 words, one bank. The grid is drawn just after the first mark so it lands
once the OLED is live (avoids the cold-start blank grid). Verified in a
cycle-accurate emulator: reproduces the manual's worked example exactly
(O:9,4,1,5 / X:3,8,6 -> 1-9-5 diagonal win), and all 729 three-move openings
terminate cleanly with no runaway and no desync. A human-first companion file
is next.
