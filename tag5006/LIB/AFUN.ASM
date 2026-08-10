$DEBUG
$NOMOD51
$INCLUDE        (\C51V4\ASM\REG52.INC)
;
;               ASSEMBLER CODED ROUTINES
;
;================================================================
;
XPUT            MACRO   DESTINATION, SOURCE
                MOV     DPTR, #DESTINATION
                MOV     A, #LOW(SOURCE)
                MOVX    @DPTR, A
                ENDM
;
LOOKUP          MACRO   TABLE, OFFSET
                ADD     A, #((TABLE-$)-(OFFSET+3))
                MOVC    A, @A+PC
                ENDM
;
;================================================================
;
ADATA      EQU  P1.0
ASEL       EQU  P1.1
ACAL       EQU  P1.2
SCL        EQU  P1.3
KBDBIT     EQU  P1.6
SPOUT      EQU  P1.7
PORTA      EQU  0FF02H
PORTB      EQU  0FF02H
;
;================================================================
;
NAME            AFUN

PUBLIC          _NADA, _BELL, _PUTCHAR, _FLAG, WATCHDOG, CLRSCR
PUBLIC          KBHIT, MAGTAB, ATODIN, RECYCLE, _RELSET
PUBLIC          _MIRROR, _LCDTRAN
EXTRN           BIT (FKEY, FSCROLL, FSTAB, OCHECK,FL2X16)
EXTRN           DATA (CURSOR, BEEPTIME, SKEY, PKEY, SCODE, PCODE, SCANCODE,BIMAGE)
EXTRN           XDATA (DISBUF, DISFLAG, CYCLAD, RELBUF, DISCOM)

?PR?AFUN        SEGMENT   CODE
;
;================================================================
;
                RSEG    ?PR?AFUN
;
;================================================================
;
;       DUMMY FUNCTION, USED FOR DEBUGGING PURPOSE ONLY
;

_NADA:          RET
;
;================================================================
;
;       WATCHDOG HANDLER
;
WATCHDOG:
                CLR    SCL
                NOP
                SETB   SCL
                RET
;
;================================================================
;
;       BASIC ONE CHARACTER DISPLAY ROUTINE
;
_PUTCHAR:       CJNE    R7, #7, NOBELL
                MOV     R7, #1
_BELL:          MOV     DPTR, #DISCOM
                MOVX    A, @DPTR
                XRL     A, R7
                ANL     A, #0F0H
                XRL     A, R7
                ORL     A, #8
                MOVX    @DPTR, A
                RET
NOBELL:         JNC     NOCURSOR
                CJNE    R7, #0, SETCURSOR
                RET
SETCURSOR:      MOV     CURSOR, R7
                RET
CLRSCR:         MOV     DPTR, #DISFLAG
                MOV     R6, #8
                CLR     A
NOMAG:          MOVX    @DPTR, A
                INC     DPL
                DJNZ    R6, NOMAG
                RET
NEWLINE:        MOV     CURSOR, #1      ;CURSOR HOME
                RET
NOCURSOR:       CJNE    R7, #10H, $+3
                JC      NEWLINE
                CJNE    R7, #20H, $+3
                JC      CLRSCR
                MOV     DPTR, #DISFLAG-1
                ANL     CURSOR, #7      ;KEEPING THE CURSOR BETWEEN LIMITS
                MOV     A, CURSOR
                JNZ     CURSOK
                INC     A
                MOV     CURSOR, A
CURSOK:         CJNE    R7, #'.', GOCHAR
                DJNZ    ACC, PERIOD      ;DECIMAL PERIOD HANDLING
                RET                      ;NO PERIOD BEFORE FIRST CHARACTER
PERIOD:         ADD     A, DPL
                MOV     DPL, A
                MOVX    A, @DPTR
                ORL     A, #20H
                MOVX    @DPTR, A
                RET
GOCHAR:         CJNE    R7, #80H, $+3    ;LOWER CASE CHARACTERS NOT ALLOWED
                JNC     NOPUT
                CJNE    A, #7, $+3       ;CHECKING THE CURSOR FOR SCROLLING
                MOV     F0, C            ;IF CURSOR = 7, F0 CLEAR FOR SCROLL
                ADD     A, DPL
                MOV     DPL, A           ;INSERTING THE 7 SEGMENTS ENCODED
                MOV     A, R7            ;CHARACTER AT THE CURSOR POSITION
                LOOKUP  MAGTAB, 20H      ;MAG DISPLAY DECODER
                JNB     F0, SCROLL       ;UNLESS IT IS AT THE END OF THE LINE
                MOVX    @DPTR, A
                INC     CURSOR
NOPUT:          RET
SCROLL:         JNB     FSCROLL, NOPUT   ;END OF LINE REACHED, SCROLL IF
                MOV     R5, #6           ;FSCROLL FLAG IS SET
                MOV     R6, A
SCRLOOP:        DEC     DPL
                MOVX    A, @DPTR
                XCH     A, R6
                MOVX    @DPTR, A
                DJNZ    R5, SCRLOOP
                RET
;
;================================================================
;                  a
;               …ÕÕÕÕÕª        Location inside byte :
;             f ∫  g  ∫ b
;               ÃÕÕÕÕÕπ         b    c    p    a    g    d    f    e
;             e ∫     ∫ c      ƒƒƒ  ƒƒƒ  ƒƒƒ  ƒƒƒ  ƒƒƒ  ƒƒƒ  ƒƒƒ  ƒƒƒ
;               »ÕÕÕÕÕº  ‹      7                                  0
;                  d     p - decimal period position
;
;
MAGTAB:         DB      000H, 003H, 082H, 0C3H, 000H, 09AH, 000H, 080H ;20H
                DB      017H, 0D4H, 04DH, 00BH, 040H, 008H, 001H, 089H
                DB      0D7H, 0C0H, 09DH, 0DCH, 0CAH, 05EH, 05FH, 0D0H ;30H
                DB      0DFH, 0DEH, 000H, 000H, 000H, 00CH, 000H, 000H
                DB      000H, 0DBH, 04FH, 017H, 0CDH, 01FH, 01BH, 057H ;40H
                DB      0CBH, 0C0H, 0C5H, 08FH, 007H, 0D3H, 049H, 0D7H
                DB      09BH, 000H, 009H, 05EH, 00FH, 0C7H, 045H, 000H ;50H
                DB      01CH, 0CEH, 09DH, 003H, 04AH, 0C0H, 010H, 004H
                DB      00FH, 0D4H, 0D9H, 0D0H, 0D1H, 0C0H, 04AH, 0D3H ;Ä,Å,Ç
                DB      047H, 080H, 04CH, 04CH, 04EH, 04DH, 04DH, 0C4H ;à,â,ã
                DB      0C4H, 0D7H, 08FH, 0D5H, 0D5H, 08BH, 08BH, 091H ;ê,ë,í
                DB      048H, 055H, 049H, 000H, 000H, 000H, 000H, 000H ;ò,ô,ö
;
;================================================================
;
;       LCD VISUAL INDICATORS HANDLING
;
_FLAG:          CLR     A
                XCH     A, R7
                MOV     DPTR, #DISFLAG+7
                LOOKUP  FLAGTAB, 0
                JBC     ACC.0, $+5
                DEC     DPL
                MOV     R4, A
                MOVX    A, @DPTR
                ANL     A, R4
                JZ      $+3
                INC     R7
                MOVX    A, @DPTR
                ORL     A, R4
                CJNE    R5, #1, MAGOUT
                MOVX    @DPTR, A
                RET
MAGOUT:         JNC     NOFLAG
                XRL     A, R4
                MOVX    @DPTR, A
NOFLAG:         RET
;
FLAGTAB:        DB      011H, 041H, 081H, 005H, 003H, 021H, 009H
                DB      008H, 002H, 080H, 010H, 040H, 020H, 004H
;
;================================================================
;
;       LCD 2x16 BYTE TRANSMIT
;
_LCDTRAN:       MOV     A, R7                   ;ACC = BYTE
                MOV     R0, #BIMAGE
LCDLOOP:        JB      FL2X16, LCDLOOP         ;WAIT FOR CLOCKHANDLER
                XCHD    A, @R0                  ;LOW(BIMAGE) = LOW(ACC)
                SETB    FL2X16
                RET
;
;================================================================
;
;       KBHIT FUNCTION (BORLAND-LIKE)
;
KBHIT:          MOV     A, SKEY                 ;SCANNING THE SERIAL KEYBOARD
                ANL     A, #3FH
                CJNE    A, SCODE, DIFCODE
PARALLEL:       MOV     A, PKEY
                XCH     A, PCODE
                XRL     A, PCODE
                JZ      KBHOUT
                MOV     A, PCODE
                JZ      KBHOUT
                MOV     SKEY, #0
                MOV     SCODE, #0
                ORL     A, #40H
GOTCODE:        MOV     SCANCODE, A
                SETB    FKEY
KBHOUT:         MOV     C, FKEY
                RET
DIFCODE:        XCH     A, SCODE
                ADD     A, SCODE
                ANL     A, #3FH
                JZ      PARALLEL
                MOV     A, SCODE
                JZ      PARALLEL
                SJMP    GOTCODE
;
;================================================================
;
;              A/D SAMPLING ROUTINE
;
ATODIN:         SETB    ASEL
                MOV     R6, #0
                MOV     A, #1
                CLR     C
ADLOOP:         ORL     C, /ADATA
                CLR     SCL
                RLC     A
                XCH     A, R6
                SETB    SCL
                RLC     A
                XCH     A, R6
                JNC     ADLOOP
                CLR     ASEL
                MOV     R7, A
                RET
;
;================================================================
;
;              FORCED TEST MODE JUMP (ADDRESS SAVED AT CYCLAD)
;
RECYCLE:        MOV     DPTR, #CYCLAD
                MOVX    A, @DPTR
                MOV     R2, A
                INC     DPTR
                MOVX    A, @DPTR
                MOV     DPH, A
                MOV     DPL, R2
                SETB    FSTAB
                CLR     A
                JMP     @A+DPTR
;
;================================================================
;
;              RELAY SETTING FROM THE INTEGER INTO R6-R7
;
_RELSET:        MOV     DPTR, #RELBUF+2
                SETB    F0
                MOV     A, R7
RELOOP:         MOV     R2, #4
RELBIT:         MOV     B, #11H
                RRC     A
                JNC     $+5
                ORL     B, #6
                RRC     A
                JNC     $+5
                ORL     B, #60H
                XCH     A, B
                MOVX    @DPTR, A
                INC     DPTR
                MOV     A, B
                DJNZ    R2, RELBIT
                MOV     A, R6
                JBC     F0, RELOOP
                RET
;
;================================================================
;
_MIRROR:       MOV     A, #80H
MIRLOOP:       XCH     A, R7
               RLC     A
               XCH     A, R7
               RRC     A
               JNC     MIRLOOP
               MOV     R7, A
               RET
;
;================================================================
;
                END
