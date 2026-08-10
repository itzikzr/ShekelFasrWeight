$NOMOD51
$INCLUDE  (FANBASE.INC)
;
;================================================================
;
IDATALEN        EQU     100H    ;LENGTH OF INTERNAL DATA (80C32)
IBPSTACK        EQU     0       ;NO SMALL REENTRANT FUNCTIONS
XBPSTACK        EQU     1       ;LARGE REENTRANT FUNCTIONS USED
PPAGEENABLE     EQU     1       ;PDATA USED FOR MEMORY MAPPED I/O PORTS
PPAGE           EQU     0       ;PDATA PAGE (MEMORY PAGE 0)

EXTRN           CODE (MAIN, STARTUP, ICSTOP, ICBYTE, ICOUT)
;EXTRN          CODE (PROGMODE, TESTMODE, CALIB, PWROFF)
EXTRN       CODE (PROGMODE, TESTMODE, CALIB, HIDEMODE, CALIBZ, PWROFF)
EXTRN           BIT (FSEND, FSYNC, FSTOP, FKEY, FDISP, FLCD, FRUN, FPOWER)
EXTRN           BIT (FRAME, FBCD, OPOL)
EXTRN           DATA (VISUAL, SCANCODE, PROG, ICMODE)

PUBLIC          ?C_STARTUP, RSIPO, RSOPO, RSMPO, RSOCO
PUBLIC          RSMODE, BIOSTICK, PPAGE, BCDBUF, ADSAMP
PUBLIC          TIMERS, BUFIN, ADBUF, ADTYPE, ADSTAT
PUBLIC          LCDBUF, DISBUF, BRUT, FUN, RSBYTE

?C_C51STARTUP   SEGMENT     CODE
?DT?REGBANK     SEGMENT     DATA
?STACK          SEGMENT     IDATA
;
;================================================================
;
;               INTERRUPT VECTORS JUMP TABLE
;
                CSEG    AT  0
?C_STARTUP:     LJMP    STARTUP1        ;RESET VECTOR

                ORG     3               ;EX0 VECTOR
                CLR     EX0
                RETI

                ORG     0BH             ;ET0 VECTOR
                CLR     ET0
                RETI

                ORG     13H             ;POWER MANAGEMENT INTERRUPT
                
                JNB     FPOWER, EOI
                ACALL   EOI
                LJMP    PWROFF

                ORG     1BH             ;ET1 VECTOR
                MOV     TH1, #-CLOCKRATE
                XCH     A, BANK2 + 7
                SJMP    CLOCK

                ORG     23H             ;ES INTERRUPT
                XCH     A, BANK1 + 7
                XCH     A, PSW
                XCH     A, R6
                SJMP    UART
                RETI

                ORG     2BH
                CLR     ET2
EOI:            RETI
;
;================================================================
;
;               UART INTERRUPT SERVICE ROUTINE
;
UART:           JBC     RI, RSIN
                JBC     TI, RSOUT
                SJMP    ISREND
RSIN:           MOV     A, SBUF
                MOVX    @R0, A
                INC     R0
                CJNE    R0, #LOW BUFIN+INSIZE, ISREND
                MOV     R0, #LOW BUFIN
                SJMP    ISREND
RSOUT:          JNB     FSEND, ISREND
                CJNE    R4, #0, RSEND   ;OUTPUT BYTE COUNTER
                CLR     FSEND           ;END OF TRANSMISSION
                SJMP    ISREND
RSEND:          DEC     R4              ;OUTPUT COUNTER
                JB      F0, BIGPO       ;REGISTER/DPTR DATA ACCESS
                MOV     A, @R1          ;REGISTER ACCESS
                JNB     F1, SMALLPO
                MOVX    A, @R1          ;EXTERNAL MEMORY (PDATA)
SMALLPO:        INC     R1
                SJMP    RSPUT
BIGPO:          XCH     A, R2           ;DPTR ACCESS
                XCH     A, DPH
                XCH     A, R2
                XCH     A, R1
                XCH     A, DPL
                XCH     A, R1
                JNB     F1, ROMPO
                MOVX    A, @DPTR
                SJMP    NEXPO
ROMPO:          CLR     A
                MOVC    A, @A + DPTR
NEXPO:          INC     DPTR
                XCH     A, R2
                XCH     A, DPH
                XCH     A, R2
                XCH     A, R1
                XCH     A, DPL
                XCH     A, R1
RSPUT:          SETPAR
                MOV     TB8, C
                MOV     SBUF, A
ISREND:         XCH     A, R6
                XCH     A, PSW
                XCH     A, BANK1 + 7
                RETI

;================================================================

;       MAIN CLOCK HANDLER

CLOCK:          XCH     A, PSW
                XCH     A, R6
                INC     R3              ;BIOSTICK RUNNING AT 2400/SECONDS
                MOV     A, R3
                JNZ     $ + 8
                INC     R2
                JNB     FSYNC, $ + 5
                SETB    FDISP

;       BCD 4/20MA HANDLING

                JB      ACC.1, ICTIME
                JB      ACC.6, BCDSER
                JNB     ACC.0, ICBUS
                MOV     BCDSHIF, BCDBUF
                MOV     BCDSHIF+1, BCDBUF+1
                SJMP    KEYIN
BCDSER:         JNB     ACC.0, BCDSTEP
                MOV     A, BCDSHIF + 1          ;BCD BIT HANDLING
                ADD     A, ACC
                MOV     BCDSHIF + 1, A
                MOV     A, BCDSHIF
                RLC     A
                MOV     BCDSHIF, A
                MOV     BCD420, C
                MOV     A, R3
KEYIN:          ANL     A, #3EH                 ;KEYBOARD DEBOUNCING
                JNZ     CLOCKOUT
                MOV     A, P1                   ;KEYBOARD SAMPLING
                CPL     A
                XRL     A, SCANCODE
                ANL     A, #7
                XRL     SCANCODE, A
                ANL     A, SCANCODE             ;ANY KEY DEPRESSED?
                JZ      CLOCKOUT
                SETB    FKEY
                SJMP    CLOCKOUT                
ICTIME:         JNB     ACC.0, ICBUS
                CLR     BCD420

;       60 TIMERS (0.1 SEC)     ACC = 0

                JB      FPOWER, $+16    ;POWER ON/OFF KEY DEBOUNCING
                JB      INT1, $+6
                MOV     PWRON, #PWRTIME             
                DJNZ    PWRON, $+7
                SETB    EX1
                SETB    FPOWER
                MOVX    A, @R0          ;.1SECOND TIMERS
                JZ      NEXTIME
                DEC     A
                MOVX    @R0, A
NEXTIME:        INC     R0
                CJNE    R0, #LOW TIMERS+60, CLOCKOUT
                MOV     R0, #LOW TIMERS
                DEC     R1
ICMORE:         INC     R1
CLOCKOUT:       XCH     A, R6
                XCH     A, PSW
                XCH     A, BANK2 + 7
                RETI

;       I2C BUS HANDLING (1200/SECONDS)

BCDSTEP:        SETB    BCD420
ICBUS:          JNB     FSYNC, CLOCKOUT
                JB      F0, ATODIN
                JNB     F1, LCDFLAG
                MOVX    A, @R1
                LCALL   ICOUT           ;BEWARE, ICOUT USES R5!
                CJNE    R1, #LOW LCDBUF+LCDSIZE-1, ICMORE
                MOV     R1, #LOW VISUAL
                CLR     F1
                SJMP    CLOCKOUT
LCDFLAG:        MOV     A, @R1
                LCALL   ICOUT           ;BEWARE, ICOUT USES R5!
                CJNE    R1, #LOW (VISUAL+1), ICMORE
                LCALL   ICSTOP
                SETB    FRAME
ADSET:          SETB    F0
                CLR     F1
                MOV     A, ICMODE
                RLC     A
                RLC     A
                ANL     A, #1CH
                ADD     A, #LOW ADBUF
                MOV     R1, A
                MOV     R4, #0
                SJMP    CLOCKOUT
ATODIN:         JB      F1, LCDCODE

;       A/D HANDLER

                MOV     A, R4
                JNZ     ADNEXT
                SETB    SCL
                JNB     SCL, CLOCKOUT
                INC     R4
                MOV     A, ICMODE               ;A/D READ REQUEST
                SETB    C
                RLC     A
                ANL     A, #0FH
                ORL     A, #ADBASE
                LCALL   ICOUT
                JC      CLOCKEND
                SETB    F1                      ;A/D FAILURE
                SJMP    ADEND
ADNEXT:         INC     R4
                ADD     A, #-4
                MOV     F1, C
                MOV     A, #-1
                LCALL   ICBYTE
                MOVX    @R1, A
                JNB     F1, ICNEXT
                MOV     A, ICMODE               ;SETTING THE SAMPLING STATUS
                ANL     A, #7
                LOOKUP  BITPOS, 0
                ORL     ADSAMP, A
                JNB     FRUN, ADEND
                MOV     A, ICMODE               ;A/D RUN MODE
                RLC     A
                SWAP    A
                XRL     A, ICMODE
                INC     ICMODE
                ANL     A, #7
                JNZ     ADSET
                DEC     ICMODE
                MOV     A, #0F8H
                ANL     ICMODE, A               
ADEND:          LCALL   ICSTOP
                JB      FSTOP, ICOFF            ;I2C BUS REQUEST FROM
                JNB     FLCD, ADSET             ;MAIN PROGRAM
                MOV     R1, #0
                SJMP    CLOCKEND
ICOFF:          CLR     FSYNC                   ;MAIN PROGRAM REQUEST
                CLR     FSTOP                   ;I2C BUS RELEASED
                CLR     FDISP
                MOV     ADSAMP, #0
                JNB     FLCD, ADSET
                MOV     R1, #0
                SJMP    CLOCKEND
LCDCODE:        MOV     A, R1
                JNZ     $ + 5
                JNB     SCL, CLOCKEND
                LOOKUP  DISPINI, 0
ICSEND:         LCALL   ICOUT                   ;BEWARE, ICOUT USES R5!
                CJNE    R1, #4, ICNEXT
                CLR     F0
                MOV     R1, #LOW LCDBUF-1
ICNEXT:         INC     R1
CLOCKEND:       XCH     A, R6
                XCH     A, PSW
                XCH     A, BANK2 + 7
                RETI

;;       I2C BUS HANDLING (1200/SECONDS)
;
;BCDSTEP:        CPL     BCD420
;ICBUS:          JNB     FSYNC, CLOCKOUT
;                JB      F0, ATODIN
;                JNB     F1, LCDFLAG
;                MOVX    A, @R1
;                LCALL   ICOUT           ;BEWARE, ICOUT USES R5!
;                CJNE    R1, #LOW LCDBUF+LCDSIZE-1, ICMORE
;                MOV     R1, #LOW VISUAL
;                CLR     F1
;                SJMP    CLOCKOUT
;LCDFLAG:        MOV     A, @R1
;                LCALL   ICOUT           ;BEWARE, ICOUT USES R5!
;                CJNE    R1, #LOW VISUAL+1, ICMORE
;                LCALL   ICSTOP
;                SETB    FRAME
;
;
;ADSET:      SETB        F0
;        MOV     A, ICMODE
;        RLC     A
;        RLC     A
;        ANL     A, #1CH
;        ADD     A, #LOW ADBUF
;        MOV     R1, A
;        MOV     R4, #0
;NOSCL:      CLR     F1
;        SJMP        CLOCKOUT
;
;;       A/D HANDLER
;
;ATODIN:     JB      F1, LCDCODE
;        MOV     A, R4
;        JNZ     ADNEXT
;        SETB        SCL
;        JNB     SCL, CLOCKOUT
;        INC     R4
;        MOV     A, ICMODE       ;A/D READ REQUEST
;        SETB        C
;        RLC     A
;        ANL     A, #0FH
;        ORL     A, #ADBASE
;        LCALL       ICOUT
;            JC      CLOCKEND
;        SETB        F1          ;A/D FAILURE
;        SJMP        ADEND
;ADNEXT:     ADD     A, #-4
;        MOV     F1, C
;        MOV     A, #-1
;        LCALL       ICBYTE
;        JB      PX0, NOSCL      ;CLOCK STRETCHING FLAG (SLAVE BUSY)
;        MOVX        @R1, A
;        INC     R4
;        JNB     F1, ICNEXT
;        MOV     A, ICMODE       ;SETTING THE SAMPLING STATUS
;        ANL     A, #7
;        LOOKUP      BITPOS, 0
;        ORL     ADSAMP, A
;        JNB     FRUN, ADEND
;        MOV     A, ICMODE       ;A/D RUN MODE
;        RLC     A
;        SWAP        A
;        XRL     A, ICMODE
;        INC     ICMODE
;        ANL     A, #7
;        JNZ     ADSET
;        DEC     ICMODE
;        MOV     A, #0F8H
;        ANL     ICMODE, A               
;ADEND:      LCALL       ICSTOP
;
;
;                JB      FSTOP, ICOFF            ;I2C BUS REQUEST FROM
;                JNB     FLCD, ADSET             ;MAIN PROGRAM
;                MOV     R1, #0
;                SJMP    CLOCKEND
;ICOFF:          CLR     FSYNC                   ;MAIN PROGRAM REQUEST
;                CLR     FSTOP                   ;I2C BUS RELEASED
;                CLR     FDISP
;                MOV     ADSAMP, #0
;                JNB     FLCD, ADSET
;                MOV     R1, #0
;                SJMP    CLOCKEND
;LCDCODE:        MOV     A, R1
;                JNZ     $ + 5
;                JNB     SCL, CLOCKEND
;                LOOKUP  DISPINI, 0
;ICSEND:         LCALL   ICOUT                   ;BEWARE, ICOUT USES R5!
;                CJNE    R1, #4, ICNEXT
;                CLR     F0
;                MOV     R1, #LOW LCDBUF-1
;ICNEXT:         INC     R1
;CLOCKEND:       XCH     A, R6
;                XCH     A, PSW
;                XCH     A, BANK2 + 7
;                RETI

DISPINI:        DB      DISPAD, 0CBH, 0F0H, 0E0H, 0
BITPOS:         DB      1, 2, 4, 8, 10H, 20H, 40H, 80H

;================================================================

;       LOW LEVEL PORT AND MEMORY INITIALIZATION

                RSEG    ?C_C51STARTUP

STARTUP1:       CLR     EA              ;NO INTERRUPT DURING INITIALIZATION
                CLR     A
                MOV     IE, A           ;DISABLING ALL INTERRUPTS
                MOV     PSW, A          ;REGISTER BANK 0, F0 = 0
                MOV     PCON, A         ;POWER CONTROL BITS SETTING
                MOV     P1, #-1         ;PORT INITIALIZATION
                MOV     P2, #PPAGE      ;FAST PAGE 0 ACCESS
                MOV     P3, #-1
                CLR     PWR             ;KEEP POWER ON
                MOV     IP, #10H        ;HIGHEST PRIORITY FOR UART
                MOV     R0, A           ;CLEARING INTERNAL AND PDATA MEMORY
CLEARMEM:       MOV     @R0, A
                DJNZ    R0, CLEARMEM
                MOV     R0, #PAGEND-1
PMEM:           MOVX    @R0, A
                DJNZ    R0, PMEM
                MOVX    @R0, A
                MOV     SP, #?STACK-1   ;INTERNAL STACK POINTER
                LCALL   EOI             ;CLEAR INTERRUPT FLIP-FLOP
                LCALL   EOI             ;AT BOTH LEVELS
                MOV     TCON, #5
                MOV     TMOD, A
                MOV     BANK1+7, #BANK1 ;BANK1 ASSIGNED TO UART INTERRUPT
                MOV     BANK1, #LOW BUFIN
                MOV     BANK2+7, #BANK2+22H ;BANK2 ASSIGNED TO CLOCK
                MOV     BANK2, #TIMERS
                MOV     PWRON, #PWRTIME ;POWER ON/OFF DEBOUNCING
                SETB    EX1
                LCALL   STARTUP         ;HIGH LEVEL INITIALIZATION
                MOV     PROG, R7
;
;================================================================
;
;               BASIC SYSTEM LOOP FOR WEIGHT PROCESSING OR TEST MODE
;
MAINLOOP:       SETB    WD
                MOV     DPTR, #WORKTABLE
                MOV     A, PROG
                ADD     A, ACC
                RLC     A
                ADD     A, PROG
                CLR     WD
                JMP     @A + DPTR
WORKTABLE:      LCALL   MAIN
;               SJMP    MAINLOOP
;               LCALL   PROGMODE
;               SJMP    MAINLOOP
;               LCALL   TESTMODE
;               SJMP    MAINLOOP
;               LCALL   CALIB
;               SJMP    MAINLOOP

    SJMP MAINLOOP
    LCALL PROGMODE
    SJMP MAINLOOP
    LCALL TESTMODE
    SJMP MAINLOOP
    LCALL CALIB
    SJMP MAINLOOP
    LCALL HIDEMODE
    SJMP MAINLOOP
    LCALL CALIBZ
    SJMP MAINLOOP
;
;***************************************************************
;
;               REGISTER BANK AND DATA STORAGE FOR INTERRUPTS
;
                RSEG    ?DT?REGBANK
BANK1:          DS      8               ;REGISTER BANK 1 FOR UART INTERRUPT
BANK2:          DS      8               ;REGISTER BANK 2 FOR CLOCK INTERRUPT
BANK3:          DS      8               ;REGISTER BANK 3
;
RSIPO           EQU     BANK1           ;ISR SERIAL INPUT POINTER
RSOPO           EQU     BANK1 + 1       ;ISR SERIAL OUTPUT POINTER (R1-R2)
RSMPO           EQU     BANK1 + 3       ;MAIN INPUT POINTER
RSOCO           EQU     BANK1 + 4       ;OUTPUT BYTE COUNTER
RSMODE          EQU     BANK1 + 7       ;F0 = 0/1 FOR RAM/CODE OUTPUT
;
BIOSTICK        EQU     BANK2 + 2       ;INCREMENTED EVERY 1/2400 SECOND
;
PWRON           EQU     BANK3           ;POWER ON/OFF DEBOUNCING
ADSAMP          EQU     BANK3 + 1       ;A/D SAMPLING STATUS
BCDSHIF         EQU     BANK3 + 2       ;BCD SHIFT REGISTER
BCDBUF          EQU     BANK3 + 4       ;BCD BUFFER
;
;***************************************************************
;
                RSEG    ?STACK

                DS      1
;
;***************************************************************
;
;               PDATA VARIABLES CLEARED AT POWER UP
;
                XSEG    AT  PPAGE

TIMERS:         DS      60              ;60 .1SEC TIMERS
BUFIN:          DS      INSIZE          ;UART INPUT BUFFER
ADBUF:          DS      32              ;A/D RECEIVED WEIGHT
ADTYPE:         DS      8               ;A/D INFORMATION TYPE
ADSTAT:         DS      8               ;A/D STATUS BYTE
LCDBUF:         DS      LCDSIZE         ;LCD DISPLAY BUFFERR
DISBUF:         DS      12              ;DISPLAY AUXILIARY ASCII BUFFER
BRUT:           DS      4               ;BRUT A/D OUTPUT
FUN:            DS      3               ;FUNCTION POINTER
RSBYTE:         DS      1               ;AVOIDING GETCOM RECURSION

PAGEND          EQU     $               ;END OF PDATA AREA
;
;================================================================
;
                END

