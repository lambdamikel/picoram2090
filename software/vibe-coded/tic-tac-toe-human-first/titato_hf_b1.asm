; ===== TIC-TAC-TOE human-first  BANK 1 (display + speech + key input/mapping) =====
; Entered via bank0's 70x switches. Draws board from R1-R9, speaks, READS+MAPS the
; key (8 9 A/4 5 6/0 1 2 -> grid 1 2 3/4 5 6/7 8 9), then returns the cell to bank0.
; Scratch: R0(key->cell), RB(col), RD(char), RE(flag/wait), RF(wait). Board R1-R9,
; counter RA, result RB-at-end are preserved (all 0xx draw-args are MOV x,x = no-ops).

.ORG 0C
BBACK_D: 700           ; switch back to bank0 (its 0D=NOP, 0E=GOTO REKIN); R0=chosen cell
  NOP                  ; 0D: +1 prefetch slot
  GOTO DISPENTRY       ; 0E: +2 switch-OUT landing

; ---- key translator + settle delay, tucked in the gap before the end stub ----
.ORG 0F
; KMAP: raw key R0 -> grid cell (1-9), or R0=0 if not in the 3x3 block.
;   keys 8 9 A -> 1 2 3 ; 4 5 6 -> 4 5 6 ; 0 1 2 -> 7 8 9
KMAP: MOV 0,B
  ANDI 3,B             ; col = key & 3
  CMPI 3,B
  BRZ KMREJ            ; col==3 (keys 3,7,B) invalid
  ANDI C,0             ; rowbase = key & 0xC
  CMPI C,0
  BRZ KMREJ            ; rowbase==C (keys C-F) invalid
  CMPI 8,0
  BRZ KMTOP
  CMPI 4,0
  BRZ KMMID
  MOVI 7,0             ; bottom row: cell = 7 + col
  ADD B,0
  RET
KMTOP: MOVI 1,0        ; top row: cell = 1 + col
  ADD B,0
  RET
KMMID: MOVI 4,0        ; middle row: cell = 4 + col
  ADD B,0
  RET
KMREJ: MOVI 0,0        ; invalid -> 0 (bank1 re-reads)
  RET
WBACK_D: CALL WAIT
  GOTO BBACK_D
WBACK_E: CALL WAIT
  GOTO BBACK_E
WAIT: MOVI 1,E         ; settle length knob (raise if it sticks in bank1; was 2, now 1)
WTO: MOVI F,F
WTI: SUBI 1,F
  CMPI 0,F
  BRZ WTID
  GOTO WTI
WTID: SUBI 1,E
  CMPI 0,E
  BRZ WTDN
  GOTO WTO
WTDN: RET

.ORG 35
BBACK_E: 700           ; switch back to bank0 (its 36=NOP, 37=GOTO ENDW)
  NOP                  ; 36: +1 prefetch slot
  GOTO ENDENTRY        ; 37: +2 switch-OUT landing

; ---- bulk ----
.ORG 38
DISPENTRY: MOVI 0,E    ; flag 0 = your-turn
  GOTO BOARDDRAW
ENDENTRY: MOVI 1,E     ; flag 1 = game-over
  GOTO BOARDDRAW
BOARDDRAW: 502
  50A
  0AA
  022
  000
  000
  0AA
  022
  077
  011
  50A
  055
  055
  000
  000
  055
  055
  077
  011
  50A
  000
  000
  077
  000
  0FF
  077
  077
  000
  50A
  000
  000
  0FF
  000
  0FF
  077
  0FF
  000
  504
  CMPI 0,1
  BRZ B1
  508
  022
  000
  MOV 1,D
  CALL CHARDR
B1: CMPI 0,2
  BRZ B2
  508
  077
  000
  MOV 2,D
  CALL CHARDR
B2: CMPI 0,3
  BRZ B3
  508
  0CC
  000
  MOV 3,D
  CALL CHARDR
B3: CMPI 0,4
  BRZ B4
  508
  022
  011
  MOV 4,D
  CALL CHARDR
B4: CMPI 0,5
  BRZ B5
  508
  077
  011
  MOV 5,D
  CALL CHARDR
B5: CMPI 0,6
  BRZ B6
  508
  0CC
  011
  MOV 6,D
  CALL CHARDR
B6: CMPI 0,7
  BRZ B7
  508
  022
  022
  MOV 7,D
  CALL CHARDR
B7: CMPI 0,8
  BRZ B8
  508
  077
  022
  MOV 8,D
  CALL CHARDR
B8: CMPI 0,9
  BRZ B9
  508
  0CC
  022
  MOV 9,D
  CALL CHARDR
B9: CMPI 0,E
  BRZ DOYOU
  GOTO DORESULT
CHARDR: CMPI 1,D
  BRZ CHX
  506
  0FF
  044
  RET
CHX: 506
  088
  055
  RET
; ---- your turn: speak YOU?, then read+map a key (re-reading until it's a 3x3 cell) ----
DOYOU: 50F
  099
  055
  0FF
  044
  055
  055
  0FF
  033
  0AA
  000
KINLOOP: KIN 0
  CALL KMAP
  CMPI 0,0
  BRZ KINLOOP          ; key not in the 3x3 block -> read again (no redraw)
  GOTO WBACK_D         ; valid cell in R0 -> settle, then hand back to bank0
; ---- game over: speak result, then hand back ----
DORESULT: CMPI 2,B
  BRZ SAYI
  CMPI 1,B
  BRZ SAYU
  50F
  044
  044
  022
  055
  011
  044
  077
  055
  0AA
  000
  GOTO WBACK_E
SAYI: 50F
  099
  044
  000
  022
  077
  055
  099
  044
  0EE
  044
  0AA
  000
  GOTO WBACK_E
SAYU: 50F
  099
  055
  0FF
  044
  055
  055
  000
  022
  077
  055
  099
  044
  0EE
  044
  0AA
  000
  GOTO WBACK_E
