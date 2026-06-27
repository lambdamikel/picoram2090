PicoRAM Tic-Tac-Toe  -  HUMAN-first, TWO-BANK (graphics + strong AI)
===================================================================

This is the deluxe human-first game: the full board-scanning AI AND the OLED
graphics, split across the PicoRAM's two memory banks so neither cramps the
other. You move first; the computer plays second.

LOADING (important)
-------------------
  * Load PICO_TITATO_HF_B0.MIC into BANK 0   (the AI logic)
  * Load PICO_TITATO_HF_B1.MIC into BANK 1   (the display + speech)
  * Set PicoRAM to TTS/SPEAK mode (for the spoken prompts)
  * Start in bank 0 at address 00

How the two banks cooperate
---------------------------
Bank 0 runs the game. Whenever it needs to show the board - on your turn and at
game over - it executes 70x to switch to bank 1. Because a bank switch leaves
the program counter and all CPU registers untouched, bank 1 picks up at the
aligned address, redraws everything straight from the board registers, speaks,
then executes 700 to drop back into bank 0. A bank switch cannot jump the PC; it
just lands at PC+1, and (because of a one-instruction prefetch) the new bank only
takes over at PC+2. So every handoff is three words: 70x ; NOP (absorbs the
prefetched old-bank word at +1) ; GOTO (redirects, at +2). Two such stubs per side.

How to play
-----------
The keypad's lower-left 3x3 block maps straight onto the board:

   keypad        board
   8 9 A   -->   top row
   4 5 6   -->   middle row
   0 1 2   -->   bottom row

Press the block key for the cell you want; keys outside the block are ignored.
On your turn the OLED shows the 3x3 grid with X (you) and O (computer) and asks
"YOU?". The computer answers at once; at the end it says "I WIN", "YOU WIN", or
"DRAW". Press any key for a new game.

The computer
------------
Real tactical play: take a win, else block your threat, else center, else a
corner, else a side. It always wins when it can and always blocks a single
threat. It is beatable only by a deliberate two-corner FORK (two threats at
once) - a never-losing version needs fork-defense that doesn't fit even one
bank. Verified exhaustively over all 489 lines: every move legal, every game
ends, bank handoff correct every time, spoken result always matches the board
(computer wins 350 / draws 127 / you can win 12 fork lines).

(A simpler single-bank version using the plain LED display is in
PICO_TITATO_HF_PACKAGE.zip.)

Settle delay
------------
The display/speech ops are buffered and processed on the Pico's 2nd core. A 70x
issued while that core is still busy gets missed, so bank 1 runs a short WAIT
loop before each switch-back (700). If it ever sticks in bank 1 ("YOU?" looping),
raise the WAIT length (the `MOVI 2,E` in bank 1 - bump the 2); if it feels
sluggish, lower it.
