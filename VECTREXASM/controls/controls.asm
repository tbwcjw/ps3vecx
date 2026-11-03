; build: lwasm --format=raw --output=controls.vec controls.asm

; standard Vectrex cartridge header
        ORG $0000
        FCC "g GCE 2026"        ; cartridge header
        FCB $80
        FDB $0000               
        FCB $F8,$50,$20,$BA     
        FCC "PS3VECX CONTROLS"
        FCB $80
        FCB 0

; intensity
DIM     EQU $3F                 ; released
BRIGHT  EQU $7F                 ; pressed / page 2 / hints

; layout
LISTX   EQU $80                 ; x -128
HEADY   EQU $70                 ; y header
ROW1    EQU $54                 ; first list row (28 below header)
HINTY   EQU $A8                 ; bottom hint line

; ram
PAGE    EQU $CA00               ; 0 = game, 1 = emu
PREV    EQU $CA01               ; btn 3+4 held

START:
        LDA #$D0
        TFR A,DP

        LDD #$0103
        STD $C81F               ; enable joy 1 axes
        LDD #$0000
        STD $C821               ; joystick 2 off
                                ; TODO: joystick 2 support

        CLR $C824              

        CLR PAGE                ; page 1
        CLR PREV

MAINLOOP:
        JSR $F192               ; wait 
        LDA #$D0
        TFR A,DP
        JSR $F2A9               ; full intensity
        JSR $F354               

        JSR $F1F8               ; read joystick
        LDA #$FF
        JSR $F1B4               ; read buttonss

CHK_COMBO:
        LDA $C80F
        ANDA #$0C               ; 2 = btn 3, 3 = btn 4
        CMPA #$0C
        BEQ COMBO_HELD
        CLR PREV                ; combo released
        BRA CHK_PAGE
COMBO_HELD:
        LDA PREV
        BNE CHK_PAGE            
        LDA #1
        STA PREV
        LDA PAGE
        EORA #1                 ; flip page
        STA PAGE

CHK_PAGE:
        LDA PAGE
        LBNE PAGE2_DRAW         ; page 2 is drawn below, then loops back

        LEAU STR_TITLE,PCR
        LDA #HEADY              ; y
        LDB #$A0                ; x
        JSR $F37A

TST_UP:
        LDA #DIM
        LDB $C81C
        CMPB #32                 ; y > +32
        BLE UP_DRAW
        LDA #BRIGHT
UP_DRAW:
        JSR $F2AB               
        LEAU STR_UP,PCR
        LDA #ROW1               ; y
        LDB #LISTX              ; x
        JSR $F37A

TST_DOWN:
        LDA #DIM
        LDB $C81C
        CMPB #-32               ; y < -32
        BGE DOWN_DRAW
        LDA #BRIGHT
DOWN_DRAW:
        JSR $F2AB
        LEAU STR_DOWN,PCR
        LDA #$40
        LDB #LISTX
        JSR $F37A

TST_LEFT:
        LDA #DIM
        LDB $C81B
        CMPB #-32               ; x < -32
        BGE LEFT_DRAW
        LDA #BRIGHT
LEFT_DRAW:
        JSR $F2AB
        LEAU STR_LEFT,PCR
        LDA #$2C
        LDB #LISTX
        JSR $F37A

TST_RIGHT:
        LDA #DIM
        LDB $C81B
        CMPB #32                ; x > +32
        BLE RIGHT_DRAW
        LDA #BRIGHT
RIGHT_DRAW:
        JSR $F2AB
        LEAU STR_RIGHT,PCR
        LDA #$18
        LDB #LISTX
        JSR $F37A

TST_B1:                         ;bit 0 of $C80F
        LDA #DIM
        LDB $C80F               
        BITB #$01
        BEQ B1_DRAW
        LDA #BRIGHT
B1_DRAW:
        JSR $F2AB
        LEAU STR_B1,PCR
        LDA #$04
        LDB #LISTX
        JSR $F37A

TST_B2:                         ;bit 1 of $C80F
        LDA #DIM
        LDB $C80F
        BITB #$02
        BEQ B2_DRAW
        LDA #BRIGHT
B2_DRAW:
        JSR $F2AB
        LEAU STR_B2,PCR
        LDA #$F0
        LDB #LISTX
        JSR $F37A

TST_B3:                         ;bit 2 of $C80F
        LDA #DIM
        LDB $C80F
        BITB #$04
        BEQ B3_DRAW
        LDA #BRIGHT
B3_DRAW:
        JSR $F2AB
        LEAU STR_B3,PCR
        LDA #$DC
        LDB #LISTX
        JSR $F37A

TST_B4:                         ; bit 3 of $C80F
        LDA #DIM
        LDB $C80F
        BITB #$08
        BEQ B4_DRAW
        LDA #BRIGHT
B4_DRAW:
        JSR $F2AB
        LEAU STR_B4,PCR
        LDA #$C8
        LDB #LISTX
        JSR $F37A

        LDA #BRIGHT
        JSR $F2AB
        LEAU STR_HINT1,PCR
        LDA #HINTY
        LDB #LISTX
        JSR $F37A

MAINDONE:
        LBRA MAINLOOP


PAGE2_DRAW:
        LDA #BRIGHT
        JSR $F2AB
        LEAU STR_TITLE2,PCR
        LDA #HEADY              ; y
        LDB #$A0                ; x
        JSR $F37A

        LDA #BRIGHT
        JSR $F2AB
        LEAU STR_SS,PCR
        LDA #ROW1
        LDB #LISTX
        JSR $F37A

        LDA #BRIGHT
        JSR $F2AB
        LEAU STR_SE,PCR
        LDA #$40
        LDB #LISTX
        JSR $F37A

        LDA #BRIGHT
        JSR $F2AB
        LEAU STR_ST,PCR
        LDA #$2C
        LDB #LISTX
        JSR $F37A

        LDA #BRIGHT
        JSR $F2AB
        LEAU STR_L1,PCR
        LDA #$18
        LDB #LISTX
        JSR $F37A


        LDA #BRIGHT
        JSR $F2AB
        LEAU STR_HINT2,PCR
        LDA #HINTY
        LDB #LISTX
        JSR $F37A

        LBRA MAINLOOP

STR_TITLE:  FCC "GAME CONTROLS"
            FCB $80
STR_UP:     FCC "UP    - L-STICK/D-PAD"
            FCB $80
STR_DOWN:   FCC "DOWN  - L-STICK/D-PAD"
            FCB $80
STR_LEFT:   FCC "LEFT  - L-STICK/D-PAD"
            FCB $80
STR_RIGHT:  FCC "RIGHT - L-STICK/D-PAD"
            FCB $80
STR_B1:     FCC "BTN 1 - CROSS"
            FCB $80
STR_B2:     FCC "BTN 2 - SQUARE"
            FCB $80
STR_B3:     FCC "BTN 3 - TRIANGLE"
            FCB $80
STR_B4:     FCC "BTN 4 - CIRCLE"
            FCB $80
STR_HINT1:  FCC "PRESS 3+4 FOR PAGE 2"
            FCB $80
STR_TITLE2: FCC "EMULATOR CONTROLS"
            FCB $80
STR_SS:     FCC "START+SELECT -  MENU"
            FCB $80
STR_SE:     FCC "SELECT       - RESET"
            FCB $80
STR_ST:     FCC "START        - PAUSE"
            FCB $80
STR_L1:     FCC "L1           - SPEED"
            FCB $80

STR_HINT2:  FCC "PRESS 3+4 FOR PAGE 1"
            FCB $80