; ===== VECTOR CLOCK - single bank (v4) =====
; Busch Microtronic 2090 + PicoRAM.  Start at 00.  ONE bank, no bank switching.
;
; Structure = the firmware's known-good "LOAD RTC CLOCK" (F6A F06 FF0) up front,
; then draw the big vector 7-segment time, then loop (GOTO 01 - PicoRAM can't
; decode a jump back to address 00, so the loop re-enters at 01, not 00).
;   F6A  show HH MM SS on the LED     F06  load the RTC     FF0  wait for a key
; Press the key when the LED shows the correct time -> it clears the OLED and
; draws HH:MM as four big 7-segment digits, then loops back to the LED.
;
; R1=min1 R2=min10 R3=hour1 R4=hour10 (captured from RC-RF).
; R0=digit x-hi (x=16*R0: 0/32/64/96)  R8=position index  R9=digit.
; (No speech yet - get the display solid first, then add the talking.)
;
START: F6A            ; 00: power-on entry, show HH MM SS once. The loop re-enters at LOOP
                      ;     (01), NOT here - PicoRAM can't serve a GOTO to address 00.
LOOP: F6A             ; 01: loop entry - refresh the LED with the freshly-read time
  F06                 ; 02: load the RTC. Its async detour + disarm race plays out ENTIRELY
                      ;     inside 0x03-0x30, so we make that whole span a harmless runway:
; 0x03-0x30 auto-fill with 000 (MOV R0,R0 = a no-op) - a long NOP "runway". However the
; F06 detour/race lands execution in that range, it can only hit no-ops and then slides
; down to the KIN at 0x31, PAST the entire 0x20-0x30 detour zone. So the KIN is served
; clean, and the 502 below can NEVER run before a real keypress.
.ORG 31
KINWAIT: FF0          ; 31: wait for your key - safely beyond the detour zone
  502                 ; 32: erase the OLED - GUARANTEED to run only after a real keypress
  MOV C,1             ; capture the confirmed time into R1-R4
  MOV D,2
  MOV E,3
  MOV F,4
  MOVI F,0            ; settle: let the async clear finish before the first draw (KNOB: colon-misfire fix)
STL: SUBI 1,0
  CMPI 0,0
  BRZ DRAW
  GOTO STL
; ---- the redraw lives well past the detour zone ----
.ORG 40
; colon: two 3-px dots at x=53
DRAW: 50A
  055
  033                 ; x1=53
  0DD
  000                 ; y1=13
  055
  033                 ; x2=53
  0FF
  000                 ; y2=15
  50A
  055
  033
  077
  011                 ; y1=23
  055
  033
  099
  011                 ; y2=25
; ---- the four digits: R8=continuation, R0=x-hi, R9=value ----
  MOVI 1,8
  MOVI 0,0
  MOV 4,9             ; hour tens at x=0
  GOTO DD
NX1: MOVI 2,8
  MOVI 2,0
  MOV 3,9             ; hour units at x=32
  GOTO DD
NX2: MOVI 3,8
  MOVI 4,0
  MOV 2,9             ; minute tens at x=64
  GOTO DD
NX3: MOVI 4,8
  MOVI 6,0
  MOV 1,9             ; minute units at x=96
  GOTO DD
; all four drawn -> loop straight back. The clock STAYS on screen through the loop
; (F6A doesn't touch the OLED, and 502 now waits behind the KIN at 0x31), so we no longer
; need a second "hold" KIN here - the one at 0x31 does it all. Single KIN, one press/round.
NX4: GOTO LOOP        ; loop back to the LED (GOTO 01 = C01, never C00)
; ---- digit dispatch ----
DD: CMPI 0,9
  BRZ D0
  CMPI 1,9
  BRZ D1
  CMPI 2,9
  BRZ D2
  CMPI 3,9
  BRZ D3
  CMPI 4,9
  BRZ D4
  CMPI 5,9
  BRZ D5
  CMPI 6,9
  BRZ D6
  CMPI 7,9
  BRZ D7
  CMPI 8,9
  BRZ D8
  GOTO D9
RETD: CMPI 1,8
  BRZ NX1
  CMPI 2,8
  BRZ NX2
  CMPI 3,8
  BRZ NX3
  GOTO NX4
; ---- digit chains (shared tails via fallthrough) ----
D8: CALL SG           ; 8 = G + 0
D0: CALL SA
  CALL SB
  CALL SC
  CALL SD
  CALL SE
  CALL SF
  GOTO RETD
D6: CALL SE           ; 6 = E + 5
D5: CALL SA
  CALL SF
  CALL SG
  CALL SC
  CALL SD
  GOTO RETD
D9: CALL SF           ; 9 = F + 3
D3: CALL SA
  CALL SB
  CALL SG
  CALL SC
  CALL SD
  GOTO RETD
D7: CALL SA           ; 7 = A + 1
D1: CALL SB
  CALL SC
  GOTO RETD
D2: CALL SA
  CALL SB
  CALL SG
  CALL SE
  CALL SD
  GOTO RETD
D4: CALL SF
  CALL SG
  CALL SB
  CALL SC
  GOTO RETD
; ---- segment leaves (x-hi injected from R0) ----
SA: 50A
  000
  3F0
  088
  000
  0AA
  3F0
  088
  000
  RET
SB: 50A
  0AA
  3F0
  088
  000
  0AA
  3F0
  033
  011
  RET
SC: 50A
  0AA
  3F0
  033
  011
  0AA
  3F0
  0EE
  011
  RET
SD: 50A
  000
  3F0
  0EE
  011
  0AA
  3F0
  0EE
  011
  RET
SE: 50A
  000
  3F0
  033
  011
  000
  3F0
  0EE
  011
  RET
SF: 50A
  000
  3F0
  088
  000
  000
  3F0
  033
  011
  RET
SG: 50A
  000
  3F0
  033
  011
  0AA
  3F0
  033
  011
  RET
