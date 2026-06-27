PicoRAM Tic-Tac-Toe  -  HUMAN-first (you go first)
==================================================

Companion to the computer-first game. Here YOU open; the computer plays second.

Load PICO_TITATO_HF.MIC, start at address 00. Uses the normal LED/digit display
(not the OLED graphics) - that is what let the board-scanning AI fit in one bank.

How to play
-----------
Cells are the number keys in a numpad layout:

   1 2 3
   4 5 6
   7 8 9

Press the digit for the cell you want. The display shows all nine cells as
digits, left to right, top to bottom:  0 = empty, 1 = you, 4 = computer.
Pick any empty (0) cell. The computer answers immediately. A beep marks the
end of the game; the final board stays on screen - press any key for a new game.

The computer
------------
It plays a real tactical strategy, in priority order: take a winning move, else
block your immediate threat, else center, else a corner, else a side. It always
takes a win and always blocks a single threat, so casual play will lose or draw.

It is NOT unbeatable: a player who sets up a classic two-corner FORK (two
threats at once) can win - the computer can only block one. Making it never
lose needs general fork-defense, and that board-scanning logic does not fit in
the 256-word bank (no indirect addressing makes every cell access a long
dispatch). Verified exhaustively over all 489 lines of play: every computer
move is legal, every game terminates, the computer wins 350 / draws 127 and can
be beaten in only 12 fork lines.

(The first-player Busch program in PICO_TITATO_CF is unbeatable because it is
compact arithmetic, not board-scanning - the second player has no such shortcut.)
