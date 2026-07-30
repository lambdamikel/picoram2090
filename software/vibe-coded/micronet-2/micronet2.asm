; ===== MICRONET 2  -  Busch Microtronic 2090 + PicoRAM =====
; A self-restarting version of the classic MICRONET demo. Each cycle:
; wipe the OLED -> draw the diagonal net (16 lines, animated, line by line)
; -> print "MICROTRONIC" in the top (yellow) band and SPEAK it (TTS echo)
; -> pause a few seconds -> start over. No keys, runs forever.
;
; R0 = top-anchor index, R1 = bottom-anchor index (x = 16*R, so 0/32/64/96),
; R2:R3:R4:R5 = a 4-digit line counter (shown on the 6-digit LED display via F60;
; its low nibble also drives the DOT output LEDs). RE/RF = delay counters.
; Net: 4 top anchors at y=8 x 4 bottom anchors at y=31.
; TTS/SOUND switch must be in TTS position for speech (prints silently otherwise).
;
; ---- startup (00 must be non-50x; 502 BEFORE the warm-up delay) ----
START: CLEAR
  DOT 0               ; first display op (cold-start nudge)
  502                 ; OLED clear FIRST; the cold-start wipe settles during warm-up
  F60                 ; LED display on: all 6 digits from R0 (anchors R0/R1 + the R2..R5 counter)
  MOVI 2,E            ; warm-up ~2.5 s so the OLED is up (cold start only)
WRM1: MOVI F,F
WRM2: SUBI 1,F
  CMPI 0,F
  BRZ WRM3
  GOTO WRM2
WRM3: SUBI 1,E
  CMPI 0,E
  BRZ NET
  GOTO WRM1
; ---- one demo cycle: the net (16 lines) ----
NET: MOVI 0,0
  MOVI 0,1
LINE: 50A             ; line (16*R0, 8) -> (16*R1, 31)
  000                 ; X1 lo
  3F0                 ; X1 hi = R0
  088                 ; Y1 = 8
  000
  000                 ; X2 lo
  3F1                 ; X2 hi = R1
  0FF                 ; Y2 = 31
  011
; ---- blinkenlights: tick a 4-digit counter R2:R3:R4:R5 and show its low nibble on the DOT LEDs ----
  ADDI 1,5            ; one tick per line drawn (R5 = units)
  BRC CY4            ; ripple the carry up through R4, R3, R2
  GOTO BLINK
CY4: ADDI 1,4
  BRC CY3
  GOTO BLINK
CY3: ADDI 1,3
  BRC CY2
  GOTO BLINK
CY2: ADDI 1,2
BLINK: DOT 5         ; 4 output LEDs = counter low nibble (blinks as each net line appears)
; ---- next line ----
  ADDI 2,0
  CMPI 8,0
  BRZ NXTB
  GOTO LINE
NXTB: MOVI 0,0
  ADDI 2,1
  CMPI 8,1
  BRZ TEXT
  GOTO LINE
; ---- title: print in the yellow band + speak ----
TEXT: 50E             ; TTS echo on (chars below also go to the speech buffer)
  506                 ; char mode; cursor is at (0,0) after the wipe
  0DD
  044                 ; M
  099
  044                 ; I
  033
  044                 ; C
  022
  055                 ; R
  0FF
  044                 ; O
  044
  055                 ; T
  022
  055                 ; R
  0FF
  044                 ; O
  0EE
  044                 ; N
  099
  044                 ; I
  033
  044                 ; C
  0AA
  000                 ; newline: cursor down + SPEAK "MICROTRONIC"
; ---- admire it for a few seconds, then start over ----
PAUSE: MOVI 4,E       ; ~4-5 s pause; THE KNOB: raise/lower for longer/shorter
PAU1: MOVI F,F
PAU2: SUBI 1,F
  DOT F               ; keep the LEDs flickering during the pause (fast countdown on RF)
  CMPI 0,F
  BRZ PAU3
  GOTO PAU2
PAU3: SUBI 1,E
  CMPI 0,E
  BRZ AGAIN
  GOTO PAU1
AGAIN: 502            ; wipe (also homes the cursor); no 0xx before the next 50x
  GOTO NET
