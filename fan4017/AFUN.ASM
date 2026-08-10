$NOMOD51
$INCLUDE  (FANBASE.INC)
;
;================================================================
;
PUBLIC   WATCHDOG, _NADA, _BTM, _BIOSTIME, _DELAY
PUBLIC   _ICOUT, _ICIN, ICSTOP, ICBYTE, ICOUT, _ADOUT
PUBLIC   _PUTCHAR, CLRSCR, _GETCH, _SETBIT, _GETBIT
EXTRN   BIT (FSCROLL, FKEY)
EXTRN   DATA (BIOSTICK, CURSOR, VISUAL, SCANCODE)
EXTRN   DATA (RSIPO, RSMPO)
EXTRN   XDATA (LCDBUF)
EXTRN   CODE (GETCOM)
;
;================================================================
;
NAME   AFUN
?PR?AFUN  SEGMENT CODE
    RSEG    ?PR?AFUN
;
;================================================================
;
WATCHDOG:  CLR  WD
    NOP
    NOP
    SETB WD
_NADA:   RET
;
;================================================================
;
; RETURNS THE VALUE OF THE BIOS TICK COUNTER ADDED TO THE INPUT INTEGER
;
_BIOSTIME:  MOV  A, BIOSTICK+1
    MOV  R2, BIOSTICK
    CJNE A, BIOSTICK+1, _BIOSTIME    ;AVOIDING PHASE ERROR
    ADD  A, R7
    MOV  R7, A
    MOV  A, R2
    ADDC A, R6
    MOV  R6, A
    RET
;
;================================================================
;
; RETURNS THE C BIT SET IF (BIOSTICK) - (R6, R7) IS POSITIVE
;
_BTM:   MOV  A, BIOSTICK+1
    MOV  R2, BIOSTICK
    CJNE A, BIOSTICK+1, _BTM  ;AVOIDING PHASE ERROR
    SUBB A, R7
    MOV  A, R2
    SUBB A, R6
    RLC  A
    CPL  C
    RET
;
;================================================================
;
; WAITS UP TO R6-R7 TICKS. IF BIT 0 OF R5 IS SET, THE DELAY CAN
; BE INTERRUPTED BY THE KEYBOARD. IF BIT 1 IS SET, THE DELAY CAN
; BE INTERRUPTED BY AN RS232 REQUEST.
;
_DELAY:   LCALL _BIOSTIME
DELOOP:   LCALL WATCHDOG
    LCALL _BTM
    MOV  A, R5
    JNB  ACC.0, $+5
    ORL  C, FKEY
    JNB  ACC.1, DELEND
    MOV  A, RSIPO
    XRL  A, RSMPO
    ADDC A, #-1
DELEND:   JNC  DELOOP
    RET
;
;================================================================
;
;       BASIC I2C ROUTINES
;
; _ICOUT SENDS ONE I2C CHARACTER, GENERATING A START BIT IF SCL IS SET
; IT RETURNS C SET FOR A RECEIVED ACK.
; _ADOUT WORKS LIKE _ICOUT, BUT WAITS 2 BIOS TICKS BEFORE RETURNING
; _ICIN READS A CHARACTER FROM THE I2C BUS. IT GENERATES AN ACK BIT IF
; THE LSB OF R7 IS SET
; ICSTOP GENERATES A STOP BIT.
; A RECEIVED ACK. HOWEVER, IT WAITS 2 BIOS TICKS BEFORE RETURNING.
;
; USED REGISTERS:       R5, R7
;
_ICOUT:         MOV     A, R7
ICOUT:          SETB    C               ;ASSEMBLER DIRECT ACCESS BY ACC
ICBYTE:         CLR     SDA             ;EVENTUAL START BIT GENERATION
                MOV     R5, #9          ;I2C BYTE TRANSMISSION/RECEPTION
                NOP
                CLR     SCL
                SJMP    ICSET
ICLOOP:         MOV     C, SDA
                CLR     SCL
ICSET:          RLC     A
                NOP
                MOV     SDA, C
                NOP
                SETB    SCL
                DJNZ    R5, ICLOOP
                MOV     C, SDA
                CLR     SCL
SDUP:           CPL     C
                SETB    SDA
                RET
_ICIN:          MOV     A, R7           ;ACK GENERATED IF LSB OF R7 SET
                RRC     A
                CPL     C
                MOV     A, #-1
                LCALL   ICBYTE
                MOV     R7, A
                RET
ICSTOP:         CLR     SDA
                NOP
                NOP
                SETB    SCL
                SJMP    SDUP
_ADOUT:         LCALL   _ICOUT          ;I2C TRANSMISSION FOLLOWED BY 3
                MOV     F0, C           ;BIOS TICKS DELAY
                MOV     R6, #0
                MOV     R7, #4
                LCALL   _DELAY
                MOV     C, F0
                RET
;
;================================================================
;
;  BASIC ONE CHARACTER DISPLAY ROUTINE
;
; USED REGISTERS:  B, R0
;
_PUTCHAR:  MOV  A, R7
    JZ  CHAREND     ;IGNORE NULL CHARACTER
    JB  ACC.7, CHAREND   ;7 BITS ASCII
    CJNE R7, #20H, ASCON
GOCHAR:   MOV  A, CURSOR    ;CURSOR CAN RANGE FROM 1
    INC  CURSOR     ;TO 6. IF 0, SET IT TO 1
    JZ  GOCHAR
    ADD  A, #LCDBUF-1
    MOV  R0, A
    CJNE A, #LCDBUF+LCDSIZE, $+3
    JC  CURSOK
    MOV  R0, #LCDBUF+LCDSIZE-1 ;END OF DISPLAY REACHED,
    MOV  CURSOR, #LCDSIZE+1  ;SCROLL OR STAY, ACCORDING
    JNB  FSCROLL, CURSOK   ;TO THE FSCROLL FLAG
SCROLL:   MOVX A, @R0
    XCH  A, B
    MOVX @R0, A
    DEC  R0
    CJNE R0, #LCDBUF-1, SCROLL
    MOV  R0, #LCDBUF+LCDSIZE-1
CURSOK:   RLC  A      ;SAVING THE C FLAG
    CJNE R7, #'.', CHARDIS
    JNB  ACC.0, CHAREND   ;NO PERIOD AFTER LAST DIGIT
    DEC  CURSOR     ;DECIMAL PERIOD SETTING
    CJNE R0, #LCDBUF, SETPER  ;REJECT BEFORE FIRST DIGIT
    RET
SETPER:   DEC  R0
    MOVX A, @R0
    ORL  A, #20H
    MOVX @R0, A
    RET
CHARDIS:  MOV  A, R7
    LOOKUP LCDTAB, 20H    ;7 SEGMENTS ENCODING
    MOVX @R0, A
CHAREND:  RET
ASCON:   JNC  GOCHAR
    XRL  A, #0AH     ;LF
    JZ  NEWLINE
    XRL  A, #0AH XOR 0DH   ;CR
    JZ  NEWLINE
CLRSCR:   MOV  R0, #LCDBUF    ;ANY OTHER CHARACTERS CLEARS
    CLR  A      ;THE SCREEN
    ANL  VISUAL, #3
    ANL  VISUAL+1, #3
BLANK:   MOVX @R0, A
    INC  R0
    CJNE R0, #LCDBUF+LCDSIZE, BLANK
NEWLINE:  MOV  CURSOR, #1
    RET
;
;================================================================
;
;    A
;   F  B    BIT POSITION INTO THE LCD BYTE
;    G     B C P A G D F E
;   E  C            P = DECIMAL PERIOD
;    D  P  


LCDTAB:   DB  000H, 003H, 082H, 0C3H, 000H, 09AH, 000H, 080H ;20H
    DB  017H, 0D4H, 04DH, 00BH, 040H, 008H, 001H, 089H
    DB  0D7H, 0C0H, 09DH, 0DCH, 0CAH, 05EH, 05FH, 0D0H ;30H
    DB  0DFH, 0DEH, 000H, 000H, 000H, 00CH, 000H, 000H
    DB  000H, 0DBH, 04FH, 017H, 0CDH, 01FH, 01BH, 057H ;40H
    DB  0CBH, 0C0H, 0C5H, 08FH, 007H, 0D3H, 049H, 0D7H
    DB  09BH, 000H, 009H, 05EH, 00FH, 0C7H, 045H, 000H ;50H
    DB  01CH, 0CEH, 09DH, 003H, 04AH, 0C0H, 010H, 004H
    DB  00FH, 0D4H, 0D9H, 0D0H, 0D1H, 0C0H, 04AH, 0D3H ;Ä,Å,Ç
    DB  047H, 080H, 04CH, 04CH, 04EH, 04DH, 04DH, 0C4H ;à,â,ã
    DB  0C4H, 0D7H, 08FH, 0D5H, 0D5H, 08BH, 08BH, 091H ;ê,ë,í
    DB  048H, 055H, 049H, 000H, 000H, 000H, 000H, 000H ;ò,ô,ö
;
;================================================================
;
;  KEYBOARD SCAN CODE TO ASCII DECODING
;
; USED REGISTERS: R7
;
_GETCH:   LCALL WATCHDOG
         MOV  A, R7
         JZ   KBD
         MOV  A, RSIPO
         CJNE A, RSMPO, RSEXIT
KBD:     JNB  FKEY, _GETCH
         MOV  A, SCANCODE
         ORL  P1, #7     ;RELEASING SOFTWARE ACTIVATED KEYS
         CLR  FKEY
         ANL  A, #7
         LOOKUP SCANTAB, 0
         MOV  R7, A
         RET
RSEXIT:  LJMP GETCOM
;
SCANTAB:  DB  0, 'ZTpPtzw'
;
;================================================================
;
;  BIT POSITIONNING FUNCTIONS

; _SETBIT RETURNS A BYTE WITH A BIT SET AT THE POSITION DEFINED
; BY THE INPUT PARAMETER IN R7. 0 IS THE LSB, AND 7 THE MSB. IF
; THE INPUT PARAMETER IS GREATER THAN 7, IT RETURNS 0.
; _GETBIT RETURNS THE POSITION OF THE LEAST SIGNIFICANT NON-ZERO
; BIT OF THE INPUT PARAMETER IN R7. IF ALL BITS ARE CLEAR, THEN
; THE RETURN VALUE IS 0FFH

; USED REGISTERS: R7

_SETBIT:  MOV     A, R7
    MOV  R7, #0
    CJNE A, #8, $+3
    JNC  BITOUT
    LOOKUP BITNUM, 0
    MOV  R7, A
    RET
_GETBIT:  MOV  A, R7
    MOV  R7, #-1
    JZ  BITOUT
BITLOOP:  INC  R7
    RRC  A
    JNC  BITLOOP
BITOUT:   RET    
;
BITNUM:   DB  1, 2, 4, 8, 10H, 20H, 40H, 80H
;
;================================================================
;
    END

