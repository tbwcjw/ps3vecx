; Standard Vectrex cartridge header
            ORG $0000

            FCC "g GCE 2026"
            FCB $80
            FDB $0000

            FDB $F850
            FDB $40D0
            FCC "ABOUT PS3VECX"
            FCB $80

            FCB 0

LINE_SPACING EQU 44

; three lines per page
LINE_TOP     EQU LINE_SPACING
LINE_MID     EQU 0
LINE_BOT     EQU -LINE_SPACING

TEXT_X       EQU -125

; timing
HOLD_FRAMES  EQU 180            ; 3 sec on
WIPE_FRAMES  EQU 20             ; 0.3 sec off
NUM_PAGES    EQU 8              ; 24 lines / 3 per page

;ram
PAGE         EQU $C880          ; page idx
COUNT        EQU $C881          ; frame counter 
PHASE        EQU $C882          ; 0 = showing, 1 = wiping
PAGE_PTR     EQU $C883          

START:
            LDS #$CBEA

            CLR PAGE
            CLR COUNT
            CLR PHASE

FRAME:
            JSR $F192           ; Wait_Recal
            JSR $F2A9           ; Intensity_7F

            LDA PHASE
            LBNE WIPE_FRAME

            JSR DRAW_PAGE

            INC COUNT
            LDA COUNT
            CMPA #HOLD_FRAMES
            LBLO FRAME

            CLR COUNT
            LDA #1
            STA PHASE
            LBRA FRAME

WIPE_FRAME:
            INC COUNT
            LDA COUNT
            CMPA #WIPE_FRAMES
            LBLO FRAME

            CLR COUNT
            CLR PHASE

            LDA PAGE
            INCA
            CMPA #NUM_PAGES
            LBLO STORE_PAGE
            CLRA                ; wrap back to page 0

STORE_PAGE:
            STA PAGE
            LBRA FRAME

DRAW_PAGE:
            LDA PAGE
            LDB #6              ; 3 pointers x 2 bytes 
            MUL                 ; D = PAGE * 6
            LDX #PAGE_TABLE
            LEAX D,X
            STX PAGE_PTR

            LDX PAGE_PTR
            LDU 0,X
            LDA #LINE_TOP
            LDB #TEXT_X
            JSR $F37A           ; Print_Str_d 

            LDX PAGE_PTR
            LDU 2,X
            LDA #LINE_MID
            LDB #TEXT_X
            JSR $F37A

            LDX PAGE_PTR
            LDU 4,X
            LDA #LINE_BOT
            LDB #TEXT_X
            JSR $F37A

            RTS

TEXT1:
            FCC "SELECT + START TO EXIT"
            FCB $80

TEXT2:
            FCC "BACK TO THE MENU AT"
            FCB $80

TEXT3:
            FCC "AT ANY TIME."
            FCB $80

TEXT4:
            FCC "PS3VECX WAS WRITTEN BY"
            FCB $80

TEXT5:
            FCC "TBWCJW. IT STARTED AS"
            FCB $80

TEXT6:
            FCC "A FORK OF JHAWTHORN'S"
            FCB $80

TEXT7:
            FCC "VECTREX EMULATOR. YOU"
            FCB $80

TEXT8:
            FCC "CAN FIND IT ON GITHUB"
            FCB $80

TEXT9:
            FCC "JHAWTHORN/VECX."
            FCB $80

TEXT10:
            FCC "I USED PS3TOOLCHAIN"
            FCB $80

TEXT11:
            FCC "PSL1GHT AND TINY3D"
            FCB $80

TEXT12:
            FCC "TO PORT TO THE PS3."
            FCB $80

TEXT13:
            FCB $80

TEXT14:
            FCC "g MIT TBWCJW 2026"
            FCB $80

TEXT15:
            FCC "g GPL JHAWTHORN 2007"
            FCB $80

TEXT16:
            FCC "FOUND A BUG?"
            FCB $80

TEXT17:
            FCC "HAVE A COMMENT?"
            FCB $80

TEXT18:
            FCC "A SUGGESTION?"
            FCB $80

TEXT19:
            FCC "GITHUB:"
            FCB $80

TEXT20:
            FCC "TBWCJW/PS3VECX"
            FCB $80

TEXT21:
            FCB $80

TEXT22:
            FCC "UPDATES ARE RELEASED"
            FCB $80

TEXT23:
            FCC "AS ZIP FILES. DON'T"
            FCB $80

TEXT24:
            FCC "REINSTALL VIA XMB PLS"
            FCB $80


PAGE_TABLE:
            FDB TEXT1
            FDB TEXT2
            FDB TEXT3           ; page 0
            FDB TEXT4
            FDB TEXT5
            FDB TEXT6           ; page 1
            FDB TEXT7
            FDB TEXT8
            FDB TEXT9           ; page 2
            FDB TEXT10
            FDB TEXT11
            FDB TEXT12          ; page 3
            FDB TEXT13
            FDB TEXT14
            FDB TEXT15          ; page 4
            FDB TEXT16
            FDB TEXT17
            FDB TEXT18          ; page 5
            FDB TEXT19
            FDB TEXT20
            FDB TEXT21          ; page 6
            FDB TEXT22
            FDB TEXT23
            FDB TEXT24          ; page 7
            
            END START