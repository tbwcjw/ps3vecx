; Build: lwasm --format=raw --output=boot.vec boot.asm

            ORG $0000

; Standard Vectrex cartridge header
            FCC "g GCE 2026"
            FCB $80
            FDB $0000       ; no music

            FDB $F850
            FDB $40E8
            FCC "PS3 VECTREX"
            FCB $80

            FDB $F850
            FDB $10DE
            FCC "EMULATOR"
            FCB $80

            FDB $F850
            FDB $E0DE
            FCC "TBWCJW"
            FCB $80

            FCB 0

START:
            JMP $E000