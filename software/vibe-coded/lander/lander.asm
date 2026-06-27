; ===== LUNAR LANDER  -  Busch Microtronic 2090 + PicoRAM =====
; R0=thrust R1=ALT R2=VEL R3=FUEL R4=lander row R5/6/7=LED copies(FUEL/VEL/ALT) R8=scratch RD=last row
; LED = ALT VEL FUEL ;  OLED = lander + thrust flame over the surface ;  TTS at touchdown.
START: CLEAR
  502                 ; wipe the screen every new game (no leftover crater)
  50A                 ; draw the ground line (once per game, here)
  000
  000
  0FF
  011
  0FF
  077
  0FF
  011
  MOVI 0,0            ; thrust = 0   (no stale flame on frame 1)
  MOVI 0,D            ; last lander row = 0
  MOVI C,1            ; ALT  = 12
  MOVI 1,2            ; VEL  = 1
  MOVI F,3            ; FUEL = 15
MAIN: CALL SHOW
  KIN 0               ; thrust -> R0
  CMP 3,0             ; FUEL < thrust ?
  BRC CAPF
  GOTO PHYS
CAPF: MOV 3,0
PHYS: SUB 0,3         ; FUEL -= thrust
  ADDI 1,2            ; VEL  += gravity
  CMP 2,0
  BRC VZ
  SUB 0,2             ; VEL -= thrust
  GOTO LCHK
VZ: MOVI 0,2
LCHK: CMP 2,1         ; VEL < ALT ?
  BRC FALL
  CMPI 2,2            ; impact VEL > 2 ?
  BRC CRASH
  GOTO LAND
FALL: SUB 2,1         ; ALT -= VEL
  GOTO MAIN
; ---------- display (ground is already there; just the lander + flame) ----------
SHOW: MOV 1,7
  MOV 2,6
  MOV 3,5
  DISP 3,5            ; LED: ALT VEL FUEL
  MOVI 3,4            ; row from ALT:  >=9->0  6..8->1  3..5->2  <=2->3
  CMPI 2,1
  BRC SR2
  GOTO ROWOK
SR2: MOVI 2,4
  CMPI 5,1
  BRC SR1
  GOTO ROWOK
SR1: MOVI 1,4
  CMPI 8,1
  BRC SR0
  GOTO ROWOK
SR0: MOVI 0,4
ROWOK: 508            ; erase old lander
  077
  3FD
  506
  000
  022
  CMPI 2,D            ; old flame off-screen if RD==3
  BRC NOEF
  MOV D,8
  ADDI 1,8
  508                 ; erase old flame
  077
  3F8
  506
  000
  022
NOEF: 508             ; draw lander
  077
  3F4
  506
  011
  044
  CMPI 0,0            ; no burn -> no flame
  BRZ NOFL
  CMPI 2,4            ; on the ground -> no flame
  BRC NOFL
  MOV 4,8
  ADDI 1,8
  508                 ; draw flame 'V'
  077
  3F8
  506
  066
  055
NOFL: 504
  MOV 4,D
  RET
; ---------- endings ----------
LAND: MOVI 0,1
  CALL SHOW
  50F
  044
  055
  088
  044
  055
  044
  000
  022
  055
  044
  011
  044
  077
  044
  0CC
  044
  055
  044
  000
  022
  088
  044
  011
  044
  033
  055
  000
  022
  0CC
  044
  011
  044
  0EE
  044
  044
  044
  055
  044
  044
  044
  0AA
  000
  GOTO OVER
CRASH: MOVI 0,1
  CALL SHOW
  508
  066
  033
  506
  0AA
  022
  508
  088
  033
  506
  0AA
  022
  508
  077
  033
  506
  088
  055
  504
  50F
  033
  044
  022
  055
  011
  044
  033
  055
  088
  044
  0AA
  000
OVER: KIN 0
  GOTO START
