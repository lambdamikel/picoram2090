; ===== TIC-TAC-TOE  (human-first, tactical 2nd-player AI) =====
; Linear cells 1-9 = number keys.  board R1..R9: 0 empty, 1 you(X), 4 me(O).
; Single-level CALL only: leaves = READ, PLACE, SCAN, POS ; main flow is GOTO-based.
; RA = move count.  RC = value/target.  RD = read/sum scratch.  R0 = cell / scan-result.
START: CLEAR
  502                  ; clear OLED early (triggers init); bank1 redraws when waiting
  MOVI 2,E             ; warm-up delay so the OLED is live before bank1's first draw
WDLY1: MOVI F,F
WDLY2: SUBI 1,F
  CMPI 0,F
  BRZ WDLY1E
  GOTO WDLY2
WDLY1E: SUBI 1,E
  CMPI 0,E
  BRZ HTURN
  GOTO WDLY1
; -------- your turn: 701 -> bank1 redraws board + says "YOU?", returns to REKIN --------
HTURN: 701
  NOP                  ; +1: prefetched word runs from the OLD bank here (absorb it)
  GOTO REKIN           ; +2: new bank has settled; GOTO redirects control
REKIN: CALL READ       ; R0 = valid cell 1..9 (bank1 read+mapped+validated the key); RD = board[R0]
  CMPI 0,D
  BRZ HFREE
  GOTO HTURN           ; occupied -> re-prompt (back to bank1: redraw + new key)
HFREE: MOVI 1,C
  CALL PLACE           ; board[R0]=1
  ADDI 1,A
  MOVI 3,C
  CALL SCAN            ; you made a line?
  CMPI 0,0
  BRZ HCHK
  GOTO YOUWIN
HCHK: CMPI 9,A
  BRZ DRAWG            ; board full
; ---------- my turn ----------
  MOVI 8,C
  CALL SCAN            ; can I win?
  CMPI 0,0
  BRZ CBLK
  GOTO CDO
CBLK: MOVI 2,C
  CALL SCAN            ; must I block?
  CMPI 0,0
  BRZ CPOS
  GOTO CDO
CPOS: CALL POS         ; center / corner / side
CDO: MOVI 4,C
  CALL PLACE           ; board[R0]=4
  ADDI 1,A
  MOVI C,C             ; target 12
  CALL SCAN            ; did I make a line?
  CMPI 0,0
  BRZ HTURN
  GOTO IWIN
; ---------- endings (RB = 1 you / 2 me / 0 draw ; wait then restart) ----------
YOUWIN: MOVI 1,B
  GOTO ENDG
IWIN: MOVI 2,B
  GOTO ENDG
DRAWG: MOVI 0,B
; ---- game over: 701 -> bank1 redraws + speaks result (uses RB), returns to ENDW ----
ENDG: 701
  NOP                  ; +1: absorb the prefetched (old-bank) word
  GOTO ENDW            ; +2: GOTO redirects after the switch settles
ENDW: KIN 0
  GOTO START
; ---------- READ: RD = board[R0] ----------
READ: CMPI 1,0
  BRZ RR1
  CMPI 2,0
  BRZ RR2
  CMPI 3,0
  BRZ RR3
  CMPI 4,0
  BRZ RR4
  CMPI 5,0
  BRZ RR5
  CMPI 6,0
  BRZ RR6
  CMPI 7,0
  BRZ RR7
  CMPI 8,0
  BRZ RR8
  MOV 9,D
  RET
RR1: MOV 1,D
  RET
RR2: MOV 2,D
  RET
RR3: MOV 3,D
  RET
RR4: MOV 4,D
  RET
RR5: MOV 5,D
  RET
RR6: MOV 6,D
  RET
RR7: MOV 7,D
  RET
RR8: MOV 8,D
  RET
; ---------- PLACE: board[R0] = RC ----------
PLACE: CMPI 1,0
  BRZ PL1
  CMPI 2,0
  BRZ PL2
  CMPI 3,0
  BRZ PL3
  CMPI 4,0
  BRZ PL4
  CMPI 5,0
  BRZ PL5
  CMPI 6,0
  BRZ PL6
  CMPI 7,0
  BRZ PL7
  CMPI 8,0
  BRZ PL8
  MOV C,9
  RET
PL1: MOV C,1
  RET
PL2: MOV C,2
  RET
PL3: MOV C,3
  RET
PL4: MOV C,4
  RET
PL5: MOV C,5
  RET
PL6: MOV C,6
  RET
PL7: MOV C,7
  RET
PL8: MOV C,8
  RET
; ---------- SCAN: RC=target ; R0=empty cell of first line summing to RC (0 if none) ----------
SCAN: MOVI 0,0
  MOV 1,D
  ADD 2,D
  ADD 3,D
  CMP C,D
  BRZ F123
  MOV 4,D
  ADD 5,D
  ADD 6,D
  CMP C,D
  BRZ F456
  MOV 7,D
  ADD 8,D
  ADD 9,D
  CMP C,D
  BRZ F789
  MOV 1,D
  ADD 4,D
  ADD 7,D
  CMP C,D
  BRZ F147
  MOV 2,D
  ADD 5,D
  ADD 8,D
  CMP C,D
  BRZ F258
  MOV 3,D
  ADD 6,D
  ADD 9,D
  CMP C,D
  BRZ F369
  MOV 1,D
  ADD 5,D
  ADD 9,D
  CMP C,D
  BRZ F159
  MOV 3,D
  ADD 5,D
  ADD 7,D
  CMP C,D
  BRZ F357
  RET
F123: CMPI 0,1
  BRZ E1
  CMPI 0,2
  BRZ E2
  GOTO E3
F456: CMPI 0,4
  BRZ E4
  CMPI 0,5
  BRZ E5
  GOTO E6
F789: CMPI 0,7
  BRZ E7
  CMPI 0,8
  BRZ E8
  GOTO E9
F147: CMPI 0,1
  BRZ E1
  CMPI 0,4
  BRZ E4
  GOTO E7
F258: CMPI 0,2
  BRZ E2
  CMPI 0,5
  BRZ E5
  GOTO E8
F369: CMPI 0,3
  BRZ E3
  CMPI 0,6
  BRZ E6
  GOTO E9
F159: CMPI 0,1
  BRZ E1
  CMPI 0,5
  BRZ E5
  GOTO E9
F357: CMPI 0,3
  BRZ E3
  CMPI 0,5
  BRZ E5
  GOTO E7
; ---------- POS: R0 = center / corner / side (first empty) ----------
POS: CMPI 0,5
  BRZ E5
  CMPI 0,1
  BRZ E1
  CMPI 0,3
  BRZ E3
  CMPI 0,7
  BRZ E7
  CMPI 0,9
  BRZ E9
  CMPI 0,2
  BRZ E2
  CMPI 0,4
  BRZ E4
  CMPI 0,6
  BRZ E6
  CMPI 0,8
  BRZ E8
  RET
; ---------- shared cell-number setters (R0 = n ; return) ----------
E1: MOVI 1,0
  RET
E2: MOVI 2,0
  RET
E3: MOVI 3,0
  RET
E4: MOVI 4,0
  RET
E5: MOVI 5,0
  RET
E6: MOVI 6,0
  RET
E7: MOVI 7,0
  RET
E8: MOVI 8,0
  RET
E9: MOVI 9,0
  RET
