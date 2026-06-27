; ===== TIC-TAC-TOE  (computer-first, graphical)  =====
; Original Busch 58-word strategy preserved verbatim; OLED graphics + key
; translation + TTS spliced around it. 128x32 OLED, spiral cells 1 2 3/8 9 4/7 6 5.
; ---- startup (00 must be non-50x; delay so the display is up first) ----
START: CLEAR
  EXRL
  CLEAR
  DOT 0
  502
  MOVI 2,E
DLO1: MOVI F,F
DLO2: SUBI 1,F
  CMPI 0,F
  BRZ DLO1E
  GOTO DLO2
DLO1E: SUBI 1,E
  CMPI 0,E
  BRZ AFTDLY
  GOTO DLO1
AFTDLY: CLEAR
; ---- game: original flow with DRAW/KT splices ----
  MOVI 9,8
  CALL DRAWO
; grid drawn AFTER the first mark (so it lands once the OLED is live) + refresh
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
  CALL DRAWO
  DISP 2,8
HKIN1: CALL SPEAKUR
  KIN 9
  CALL KT
  CALL DRAWX
L07: ADDI 1,A
  CMPI 4,A
  BRZ EE
  MOV 9,D
  ADDI 1,D
  CALL BERE
  MOV C,8
  CALL DRAWO
HKIN2: CALL SPEAKUR
  KIN 9
  CALL KT
  CALL DRAWX
  MOV 8,D
  ADDI 4,D
  CALL BERE
  CMP 9,C
  BRZ L07
  MOV C,8
  CALL DRAWO
  MOVI C,A
  MOVI C,B
  DISP 4,8
  CALL SPEAKIWIN
L18: MOVI 0,F
  DOT F
  ADDI 1,6
  BRC L1F
  MOVI F,F
  DOT F
  GOTO L18
L1F: KIN 0
  GOTO START
EE: MOV 9,D
  ADDI 5,D
  CALL BERE
  MOV C,8
  CALL DRAWO
  MOVI E,A
  MOVI E,B
  MOVI F,F
  DOT F
  DISP 4,8
  CALL SPEAKD
  GOTO L1F
; ---- BERE: Busch move generator, verbatim ----
BERE: MOV D,C
  SUBI 1,D
  MOVI 0,E
  HXDZ
  MOV D,0
  MOV E,1
  EXRL
  MOVI 8,0
  DIV
  EXRL
  MOVI 0,1
  MOVI 8,0
  MULT
  SUB 0,C
  RET
; ---- KT: keypad 8 9 A / 4 5 6 / 0 1 2  ->  spiral cell 1 2 3 / 8 9 4 / 7 6 5 ----
KT: MOV 9,B
  ANDI 3,B
  ANDI C,9
  CMPI 8,9
  BRZ KTOP
  CMPI 4,9
  BRZ KMID
  MOVI 7,9
  SUB B,9
  RET
KTOP: MOV B,9
  ADDI 1,9
  RET
KMID: CMPI 2,B
  BRZ KM2
  MOV B,9
  ADDI 8,9
  RET
KM2: MOVI 4,9
  RET
; ---- DRAW: O at R8, X at R9 ; dispatch spiral cell R7 ----
DRAWO: MOV 8,7
  MOVI 5,6
  GOTO DRAW
DRAWX: MOV 9,7
  MOVI 1,6
DRAW: CMPI 1,7
  BRZ DC1
  CMPI 2,7
  BRZ DC2
  CMPI 3,7
  BRZ DC3
  CMPI 4,7
  BRZ DC4
  CMPI 5,7
  BRZ DC5
  CMPI 6,7
  BRZ DC6
  CMPI 7,7
  BRZ DC7
  CMPI 8,7
  BRZ DC8
  CMPI 9,7
  BRZ DC9
  RET
DC1: 508
  022
  000
  GOTO CHARDR
DC2: 508
  077
  000
  GOTO CHARDR
DC3: 508
  0CC
  000
  GOTO CHARDR
DC4: 508
  0CC
  011
  GOTO CHARDR
DC5: 508
  0CC
  022
  GOTO CHARDR
DC6: 508
  077
  022
  GOTO CHARDR
DC7: 508
  022
  022
  GOTO CHARDR
DC8: 508
  022
  011
  GOTO CHARDR
DC9: 508
  077
  011
CHARDR: CMPI 1,6
  BRZ CHX
  506
  0FF
  044
  RET
CHX: 506
  088
  055
  RET
; ---- TTS announcements (PicoRAM TTS mode). char = lo,hi  (ascii=lo+16*hi); 0AA,000 = newline=speak ----
SPEAKIWIN: 50F
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
  RET
SPEAKUR: 50F
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
  RET
SPEAKD: 50F
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
  RET
