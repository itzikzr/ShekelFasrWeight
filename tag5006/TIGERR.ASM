$DEBUG
$NOMOD51
$INCLUDE        (\C51V4\ASM\REG52.INC)
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
SETPAR          MACRO
                JNB     FBIT, $+14
                JB      FDIS, $+19
                MOV     C, P
                JNB     FODD, $+15
                CPL     C
                SJMP    $+11
                MOV     C, FODD
                RLC     A
                MOV     C, P
                ORL     C, FDIS
                RRC     A
                SETB    C
                ENDM
;
;================================================================
;
IDATALEN        EQU     80H     ; LENGTH OF INTERNAL DATA (80C31)
IBPSTACK        EQU     0       ; NO SMALL REENTRANT FUNCTIONS
XBPSTACK        EQU     1       ; LARGE REENTRANT FUNCTIONS USED
PPAGEENABLE     EQU     1       ; PDATA USED FOR MEMORY MAPPED I/O PORTS
PPAGE           EQU     0FFH    ; PDATA PAGE (I/O BASE ADDRESS)
XLEN            EQU     16      ; XDATA STACK LENGTH + 2
;
WD              EQU     P1.3    ; WATCHDOG PULSE
P1STANDBY       EQU     0DDH    ; P1 STANDBY VALUE
PBSTANDBY       EQU     0       ; PORTB STANDBY (LCD OFF, LED OFF)
CLOCKRATE       EQU     384     ; CLOCK INTERRUPT HANDLER (2400/SEC) 384
TIMENUM         EQU     32      ; NUMBER OF TIMERS (0.1 SEC)   16
FASTNUM         EQU     8       ; NUMBER OF FAST TIMERS
TIMERATE        EQU     2400/(TIMENUM*10)
BOUNCETIME      EQU     48      ; PUSHBUTTONS DEBOUNCING TIME
KEYMASK         EQU     1FH
KBDBIT          EQU     P1.6    ; SERIAL KEYBOARD BIT
SPOUT           EQU     P1.7    ; SETPOINT SERIAL BIT (RELCON)
MAGSCL          EQU     P1.4    ; MAG DISPLAY SCL BIT
MAGSDA          EQU     P1.5    ; MAG DISPLAY SDA BIT
XOUT            EQU     46H     ; EXTERNAL UART TRANSMITTER DISABLE
INSIZE          EQU     128      ; UART INPUT BUFFER SIZE
XINSIZE         EQU     16      ; EXTERNAL UART INPUT BUFFER
F1              EQU     PSW.1   ; F1 USER FLAG

NAME            ?C_STARTUP

EXTRN           CODE (?C_START, PROCESS, TESTMODE, NOVDEF, SWITCHOFF)
EXTRN           XDATA (PHASE, NOVDATA, CYCLAD)
EXTRN           DATA (?C_XBP, DISREG, BIMAGE, ADTIME)
EXTRN           BIT (FSAMP, FSEND, FXSEND, FBIT, FDIS, FODD)
EXTRN           BIT (FLCDON, FLCDGO, FTEST,FSP,FL2X16)
EXTRN           BIT (OBUSY, OPOLAR)

PUBLIC          ?C_STARTUP, MEMSIZE, PORTA, PORTB
PUBLIC          XUARTD, XUARTC, TIMERS, FASTIMERS, RSMODE
PUBLIC          BEEPTIME, SKEY, PKEY, UCOUNT, XCOUNT
PUBLIC          RECEIVE, TRANSMIT, XRECEIVE, XTRANSMIT, RELBUF
PUBLIC          INBUF, BUFEND, XINBUF, XBUFEND, DISBUF, DISFLAG
PUBLIC          DISLED, DISCOM

?C_C51STARTUP   SEGMENT   CODE
?PR?INTERRUPT   SEGMENT   CODE
?DT?REGBANK     SEGMENT   DATA
?STACK          SEGMENT   IDATA
?XD?MEMSTART    SEGMENT   XDATA
?DT?TIGERD      SEGMENT   DATA
?XD?TIGERD      SEGMENT   XDATA
;
;================================================================
;
;               INTERRUPT VECTORS JUMP TABLE
;
                CSEG    AT      0
?C_STARTUP:     LJMP    STARTUP1        ;RESET VECTOR

                ORG     3               ;EX0 VECTOR
                XCH     A,PSW2          ;BANK SWITCHING
                XCH     A,PSW
                XCH     A,R7
                LJMP    EXTUART         ;EXTERNAL UARHANDLER

                ORG     0BH             ;ET0 VECTOR
                SETB    MAGSCL
                ORL     TL0, #LOW(-CLOCKRATE)
                AJMP    CLOCKHANDLER

                ORG     13H             ;EX1 VECTOR
                LCALL   SWITCHOFF       ;POWER SHUTDOWN
                RETI

                ORG     1BH             ;ET1 VECTOR
                CLR     ET1
EOI:            RETI

                ORG     23H             ;ES INTERRUPT
                XCH     A,PSW3          ;BANK SWITCHING
                XCH     A,PSW
                XCH     A,R7
                LJMP    INTUART         ;INTERNAL UART HANDLER

;               ORG     2BH
;               CLR     ET2
;               RETI
;
;================================================================
;
;               LOW LEVEL PORT AND MEMORY INITIALIZATION
;
                RSEG    ?C_C51STARTUP

STARTUP1:       CLR     EA              ;NO INTERRUPT DURING INITIALIZATION
                CLR     A
                MOV     PSW,A           ;REGISTER BANK 0, F0 = 0
                MOV     IE,A            ;DISABLING ALL INTERRUPTS
                MOV     P1,#P1STANDBY   ;PORT INITIALIZATION
                MOV     DPTR,#PORTB     ;LCD AND LED OFF
                MOV     A,#PBSTANDBY
                MOVX    @DPTR,A
                MOV     BIMAGE,A
                MOV     IP,#2           ;INTERRUPT PRIORITY LEVEL
                MOV     PCON,#80H       ;POWER CONTROL BITS SETTING
                MOV     TCON,#4         ;INT1 = EDGE, INT0 = LEVEL
                MOV     TMOD,#21H       ;TIMER1 = BAUD, TIMER0 = CLOCK
                CLR     A
                MOV     P2,A            ;FOR EXTERNAL MEMORY PAGE 0 ACCESS
                MOV     R0,#IDATALEN - 1        ;CLEARING INTERNAL MEMORY
IDATALOOP:      MOV     @R0,A
                DJNZ    R0,IDATALOOP
                MOV     SP,#?STACK-1            ;INTERNAL STACK POINTER
                LCALL   EOI                     ;CLEAR INTERRUPT FLIP-FLOP
                LCALL   EOI                     ;AT BOTH LEVELS
                MOV     DPTR,#NOVDATA   ;CHECKING IF WARM REBOOT
                MOVX    A,@DPTR
                MOV     R2,A
                INC     DPTR
                MOVX    A,@DPTR
                MOV     R3,A
                MOV     DPTR,#NOVDEF
                CLR     A
                MOVC    A,@A+DPTR
                XCH     A,R4
                INC     DPTR
                MOVC    A,@A+DPTR
                XRL     A,R3
                XCH     A,R4
                XRL     A,R2
                ORL     A,R4
                CJNE    A,#1,$+3        ;C SET FOR A WARM REBOOT
                MOV     FTEST,C
                MOV     DPTR,#((MEMSIZE+XLEN)+100H) ;CLEARING EXTERNAL MEMORY
                CLR     A
XDATAPAGE:      DEC     DPH
                CLR     WD
XDATALOOP:      DEC     DPL
                MOVX    @DPTR,A
                CJNE    A,DPL,XDATALOOP
                SETB    WD
                CJNE    A,DPH,XDATAPAGE
                MOV     ?C_XBP,#HIGH (MEMSIZE+XLEN)  ;TOP OF EXTERNAL STACK
                MOV     ?C_XBP+1,#LOW (MEMSIZE+XLEN) ;FOR REENTRANT FUNCTIONS
                MOV     DPH,#HIGH (MEMSIZE+XLEN)     ;COMPUTING THE SIZE
MEMLOOP:        INC     DPH                 ;OF BATTERY PROTECTED EXTERNAL
                MOV     A,DPH               ;MEMORY. THE SIZE WILL BE RE-
                RLC     A                   ;TURNED IN THE "MEMSIZE"
                JC      MEMOK               ;VARIABLE, AT THE BOTTOM OF THE
                LCALL   CHECKMEM            ;EXTERNAL STACK
                JNC     MEMLOOP
                CLR     A
                MOVX    @R0,A               ;ADDRESS 0 HAS BEEN USED FOR
                JNB     F0,MEMOK            ;ROLLOVER TEST
                MOV     DPH,#HIGH (MEMSIZE+XLEN)        ;BAD MEMORY, RETURN
                MOV     A,#LOW (MEMSIZE+XLEN)           ;SIZE 0
MEMOK:          CLR     C
                SUBB    A,#LOW (MEMSIZE+XLEN)
                MOV     R1,A
                MOV     A,DPH
                SUBB    A,#HIGH (MEMSIZE+XLEN)
                MOV     DPTR,#MEMSIZE
                MOVX    @DPTR,A             ;STORE MEMORY SIZE AT MEMSIZE
                INC     DPTR
                MOV     A,R1
                MOVX    @DPTR,A
                XPUT    TIMEPO, TIMERS-1     ;POINTERS INITIALIZAITON IN
                XPUT    FASTPO, FASTIMERS-1  ;EXTERNAL MEMORY
                MOV     PSW1, #8             ;INTERRUPT BANK REGISTERS SETUP
                MOV     PSW2, #10H
                MOV     PSW3, #18H
                MOV     BANK1+1, #LOW(PORTB)
                MOV     BANK1+2, #TIMERATE
                MOV     BANK2, #LOW(XUARTC)
                MOV     R0, #DISBUF          ;DISPLAY INITIALIZATION
                MOV     R2, #0
MAGLOOP:        MOV     A, R2
                LOOKUP  MAGINI, 0
                MOVX    @R0, A
                INC     R0
                INC     R2
                CJNE    R2, #6, MAGLOOP
RECYCLE:        MOV     P2,#PPAGE            ;EXTERNAL MEMORY PAGE FOR PDATA
                MOV     DPTR,#GOTEST         ;SAVING THE RECYCLING ADDRESS
                MOV     R2, DPH              ;AT CYCLAD
                MOV     A, DPL
                MOV     DPTR,#CYCLAD
                MOVX    @DPTR,A
                MOV     A,R2
                INC     DPTR
                MOVX    @DPTR,A
;
;================================================================
;
;               BASIC SYSTEM LOOP FOR WEIGHT PROCESSING OR TEST MODE
;
                LCALL    ?C_START               ;HIGH LEVEL INITIALIZATION
                JC       GOTEST
MAINLOOP:       LCALL    PROCESS                ;WORKING LOOP
                JNB      FTEST,MAINLOOP
GOTEST:         MOV      SP,#?STACK-1           ;FORCED TEST MODE JUMP
                SETB     FTEST
                MOV      DPTR, #PHASE
                CLR      A
                MOVX     @DPTR, A
TESTLOOP:       LCALL    TESTMODE               ;TEST LOOP
                SJMP     TESTLOOP
;
MAGINI:         DB       LOW (DISFLAG+9), 70H, 0DBH, 0F0H, 0E0H, 0
;
;================================================================
;
;               PAGE MEMORY TEST ROUTINE (ADDRESSED BY DPTR)
;
CHECKMEM:       MOVX    A,@DPTR                 ;SAVING FIRST THE CHECK BYTE
                MOV     R1,A
                MOV     A,#55H
                CLR     WD
CHECKLOOP:      MOV     B,A
                MOVX    @DPTR,A
                MOVX    A,@R0                   ;CHECKING MEMORY ROLLOVER
                JNZ     ROLLOVER
                MOVX    A,@DPTR
                CJNE    A,B,BADMEM
                CPL     A
                CJNE    A,#55H,CHECKLOOP
                SJMP    MEMOUT
BADMEM:         SETB    F0                      ;BAD MEMORY FLAG
ROLLOVER:       SETB    C
MEMOUT:         SETB    WD
                MOV     A,R1
                MOVX    @DPTR,A
                RET
;
;================================================================
;
;               INTERRUPT HANDLERS
;
                RSEG    ?PR?INTERRUPT
;
;               CLOCK HANDLER
;
CLOCKHANDLER:   ORL     TH0, #HIGH(-CLOCKRATE)
                XCH     A, PSW1         ;BANK SWITCHING
                XCH     A, PSW
                XCH     A, R6
                MOV     P2, #0          ;MEMORY ACCESS FOR R0
                DJNZ    R2, MAGFAST
                MOV     R2, #TIMERATE
                MOV     R0, #LOW(TIMEPO)
                MOVX    A, @R0
                INC     A
                CJNE    A, #(TIMERS+TIMENUM), MAGNEXT
                MOV     A, #LOW(TIMERS)
MAGNEXT:        MOVX    @R0, A
                MOV     R0, A
                MOVX    A, @R0
                JZ      MAGFAST
                DEC     A
                MOVX    @R0, A
MAGFAST:        MOV     R0, #LOW(FASTPO)
                MOVX    A, @R0
                INC     A
                CJNE    A, #(FASTIMERS+FASTNUM), FASTER
                MOV     A, #LOW(FASTIMERS)
FASTER:         MOVX    @R0, A
                MOV     R0, A
                MOVX    A, @R0
                JZ      MAGSAMP
                DEC     A
                MOVX    @R0, A
MAGSAMP:        DJNZ    ADTIME, GOREL
                MOV     ADTIME, ADTIME+1
                SETB    FSAMP
GOREL:          ;JB      NEWREL, RELEND          ;BYPASS IF IT IS NEW RELBOX
                MOV     R0,#RELBUF              ;RELCON SERIAL BIT HANDLING
                MOVX    A, @R0
                ADD     A, ACC
                ORL     C, FSP
                MOV     SPOUT, C
                JNZ     RELOUT
                INC     R0
                MOVX    A, @R0
                DJNZ    ACC, $+7
                MOV     A, #8
                CPL     FSP
                MOVX    @R0, A
                ADD     A, R0
                MOV     R0, A
                MOVX    A, @R0
                MOV     R0, #RELBUF
RELOUT:         MOVX    @R0, A
RELEND:
MAGFRAME:       JNB     F1, MAGPULSE    ;MAG LCD FRAME STOP/START SYNC BITS
                CPL     MAGSDA          ;STOP/START BIT GENERATION
                JB      MAGSDA, MAGKEY
                JBC     F1, MAGKEY
MAGPULSE:       CLR     MAGSCL
                JNB     OBUSY, MAGBIT   ;CHECK BUSY LINE FOR PRINTER
                MOV     C, OPOLAR       ;ON : MARK  - READY, SPACE - BUSY
                SUBB    A, ACC          ;OFF: SPACE - READY, MARK  - BUSY
                MOV     C, RXD
                ADDC    A, #1
                RRC     A               ;OPOLAR = RXD = 0 ==> ES = 1
                MOV     ES, C           ;OPOLAR = RXD = 1 ==> ES = 1
MAGBIT:         MOV     R0, #DISPO      ;NEXT MAG BIT FROM DISPO, USED AS
                MOVX    A, @R0          ;THE MAG SHIFT REGISTER FOR 9 BITS:
                ADD     A, ACC          ;THE LAST BIT IS SET TO ALLOW ACK
                JNZ     MAGPUT
                JBC     CY, MAGPUT
                INC     R0              ;NEXT BYTE FRAME
                MOVX    A, @R0
                INC     A
                CJNE    A, #DISFLAG+10, MAGBYTE  ;AT THE END OF THE FRAME
                SETB    F1              ;RESET THE POINTER (DISBUF), AND
                MOV     A, #DISBUF      ;GENERATE A STOP/START SYNC SEQUENCE
                SJMP    MAGPUT
MAGBYTE:        MOVX    @R0, A
                MOV     R0, A
                MOVX    A, @R0
                CJNE    R0, #DISCOM, MAGSET
                MOV     C, ACC.3        ;DISPLAY COMMAND BYTE TO SEND
                CLR     ACC.3           ;CLEAR THE COMMAND BIT AFTER
                MOVX    @R0, A          ;TRANSMISSION
                MOV     ACC.3, C
                SETB    C
MAGSET:         MOV     R0, #DISPO
                RLC     A
MAGPUT:         MOVX    @R0, A
                MOV     MAGSDA, C
                INC     KEYCOUNT        ;SERIAL KEYBOARD COUNTER
                MOV     C, KBDBIT       ;RISING EDGE DETECTION
                JNC     MAGNOT
                JB      F0, MAGNOT
                CLR     A
                XCH     A, KEYCOUNT
                MOV     R4, A
MAGNOT:         MOV     F0, C
MAGKEY:         MOV     P2, #PPAGE      ;RESTORE I/O ACCESS AND
                DJNZ    DEBOUNCE, XDISPLAY
                MOVX    A, @R1
                CPL     A
                ANL     A, #KEYMASK
                MOV     R5, A
                MOV     DEBOUNCE, #BOUNCETIME
XDISPLAY:       MOV     A, BIMAGE
                MOVX    @R1, A
                JNB     FL2X16, MAGOUT
                ORL     A, #20H
                MOVX    @R1, A
                CLR     FL2X16
                XRL     A, #20H
                MOVX    @R1, A
MAGOUT:         XCH     A,R6
                XCH     A,PSW
                XCH     A,PSW1
                RETI
;
;***************************************************************
;
;               INTERNAL UART HANDLER
;
INTUART:        JBC     RI, UARTIN
                CLR     TI
                MOV     C, OBUSY        ;CHECK BUSY LINE FOR PRINTER "CITIZEN"
                CPL     C
                MOV     ES, C           ;DISABLE INTERRUPT FOR OBUSY = ON
                MOV     A, R0           ;BINARY/ASCII CHECK
                JB      FSEND, UARTON
                SETB    FSEND           ;START OF TRANSMISSION
                CLR     T1              ; RS485  
                MOV     C, ACC.3        ;UPDATE THE BINARY MODE FLAG
                MOV     F0, C
UARTON:         JNB     F0, UBRANCH
                CJNE    R6, #0, UBRANCH ;BINARY MODE, CHECK IF COUNTER 0
                CJNE    R5, #0, UBRANCH
                SJMP    EOT
UBRANCH:        JNB     ACC.1, DCBUF    ;MEMORY TYPE DISICRIMATION
DXBUF:          XCH     A, R1           ;CODE OR EXTERNAL MEMORY BUFFER
                XCH     A, DPL          ;LOAD AND SAVE THE DATA POINTER
                XCH     A, R2
                XCH     A, DPH
                XCH     A, R1
                JB      ACC.1, $+7
                CLR     A
                MOVC    A, @A+DPTR
                SJMP    $+3
                MOVX    A, @DPTR        ;GOT BYTE TO TRANSMIT
                INC     DPTR            ;INCREMENT BUFFER POINTER
                XCH     A, R1           ;AND RESTORE MAIN POINTER
                XCH     A, DPH
                XCH     A, R2
                XCH     A, DPL
                XCH     A, R1
                JB      F0, UARTGO      ;IN ASCII MODE, STOP AT NULL BYTE
                JNZ     UARTGO
EOT:            CLR     FSEND
                SETB    T1
                SJMP    UARTEND
DCBUF:          ANL     A, #5           ;DATA OR CODE BUFFER
                JNB     P, DXBUF
                MOV     A, @R1          ;DATA BUFFER
                INC     R1
                JB      F0, UARTGO      ;IN ASCII MODE, STOP AT NULL BYTE
                JZ      EOT
UARTGO:         SETPAR
                MOV     TB8, C
                MOV     SBUF, A
                JNB     F0, UARTEND     ;IN BINARY MODE, DECREMENT THE
                CJNE    R6, #0, $+4
                DEC     R5
                DEC     R6
UARTEND:        XCH     A, R7
                XCH     A, PSW
                XCH     A, PSW3
                RETI
;
UARTIN:         MOV     A, SBUF
                
                JB      FSEND, UARTEND  ;RS485 FIX BUG (ITZIK)

                XCH     A, R4
                XCH     A, DPL
                XCH     A, R3
                XCH     A, DPH
                XCH     A, R4
                JB      FBIT, $+5       ;STRIPING PARITY BIT FOR 7 BITS
                ANL     A, #7FH          ;FORMAT
                MOVX    @DPTR, A
                INC     DPTR
                XCH     A, R4
                XCH     A, DPH
                XCH     A, R3
                XCH     A, DPL
                XCH     A, R4
                CJNE    R4, #LOW(BUFEND), UARTEND
                CJNE    R3, #HIGH(BUFEND), UARTEND
                MOV     R4, #LOW(INBUF)
                MOV     R3, #HIGH(INBUF)
                SJMP    UARTEND
;
;***************************************************************
;
;               EXTERNAL UART HANDLER
;
EXTUART:        MOVX    A, @R0
                JB      ACC.7, XUARTIN
                MOV     A, RSMODE       ;BINARY/ASCII CHECK
                JB      FXSEND, XUARTON
                SETB    FXSEND          ;START OF TRANSMISSION
                MOV     C, ACC.7        ;UPDATE THE BINARY MODE FLAG
                MOV     F0, C
XUARTON:        JNB     F0, XUBRANCH
                CJNE    R6, #0, XUBRANCH ;BINARY MODE, CHECK IF COUNTER 0
                CJNE    R5, #0, XUBRANCH
                SJMP    XEOT
XUBRANCH:       JNB     ACC.5, XDCBUF   ;MEMORY TYPE DISICRIMATION
XDXBUF:         XCH     A, R1           ;CODE OR EXTERNAL MEMORY BUFFER
                XCH     A, DPL          ;LOAD AND SAVE THE DATA POINTER
                XCH     A, R2
                XCH     A, DPH
                XCH     A, R1
                JB      ACC.5, $+7
                CLR     A
                MOVC    A, @A+DPTR
                SJMP    $+3
                MOVX    A, @DPTR        ;GOT BYTE TO TRANSMIT
                INC     DPTR            ;INCREMENT BUFFER POINTER
                XCH     A, R1           ;AND RESTORE MAIN POINTER
                XCH     A, DPH
                XCH     A, R2
                XCH     A, DPL
                XCH     A, R1
                JB      F0, XUARTGO     ;IN ASCII MODE, STOP AT NULL BYTE
                JNZ     XUARTGO
XEOT:           CLR     FXSEND
                MOV     A, #XOUT
                MOVX    @R0, A
                SJMP    XUARTEND
XDCBUF:         ANL     A, #50H         ;DATA OR CODE BUFFER
                JNB     P, XDXBUF
                MOV     A, @R1          ;DATA BUFFER
                INC     R1
                JB      F0, XUARTGO     ;IN ASCII MODE, STOP AT NULL BYTE
                JZ      XEOT
XUARTGO:        DEC     R0
                MOVX    @R0, A
                INC     R0
                JNB     F0, XUARTEND    ;IN BINARY MODE, DECREMENT THE
                CJNE    R6, #0, $+4
                DEC     R5
                DEC     R6
XUARTEND:       XCH     A, R7
                XCH     A, PSW
                XCH     A, PSW2
                RETI
;
XUARTIN:        DEC     R0
                MOVX    A, @R0
                INC     R0
                XCH     A, R4
                XCH     A, DPL
                XCH     A, R3
                XCH     A, DPH
                XCH     A, R4
                MOVX    @DPTR, A
                INC     DPTR
                XCH     A, R4
                XCH     A, DPH
                XCH     A, R3
                XCH     A, DPL
                XCH     A, R4
                CJNE    R4, #LOW(XBUFEND), XUARTEND
                CJNE    R3, #HIGH(XBUFEND), XUARTEND
                MOV     R4, #LOW(XINBUF)
                MOV     R3, #HIGH(XINBUF)
                SJMP    XUARTEND
;
;***************************************************************
;
;             REGISTER BANK AND DATA STORAGE FOR INTERRUPTS
;
                RSEG    ?DT?REGBANK
BANK1:          DS      7               ;REGISTER BANK FOR CLOCK HANDLER
PSW1:           DS      1               ;MAIN_ACC/INTERRUPT_PSW IN BANK1
BANK2:          DS      1               ;REGISTER BANK FOR EXTERNAL UART
XTRANSMIT:      DS      2               ;TRANSMIT BUFFER POINTER
XRECEIVE:       DS      2               ;RECEIVE BUFFER POINTER
XCOUNT:         DS      2               ;TRANSMIT BUFFER COUNTER
ACC2:           DS      1               ;INTERRUPT ACC / MAIN PSW
BANK3:          DS      1               ;REGISTER BANK FOR INTERNAL UART
TRANSMIT:       DS      2               ;TRANSMIT BUFFER POINTER
RECEIVE:        DS      2               ;RECEIVE BUFFER POINTER
UCOUNT:         DS      2               ;TRANSMIT BUFFER COUNTER
ACC3:           DS      1               ;INTERRUPT ACC / MAIN PSW

BEEPTIME        EQU     BANK1+3         ;BEEPER TIMER
SKEY            EQU     BANK1+4         ;SERIAL KEYBOARD SCAN CODE
PKEY            EQU     BANK1+5         ;DEBOUNCED PARALLEL KEYBOARD
RSMODE          EQU     BANK3           ;TRANSMIT MODE AND POINTER TYPE

                RSEG    ?DT?TIGERD

KEYCOUNT:       DS      1               ;SERIAL KEYBOARD COUNTER
DEBOUNCE:       DS      1               ;DEBOUNCING COUNTER
PSW2:           DS      1               ;BANK2 PSW FOR INTERRUPT
PSW3:           DS      1               ;BANK3 PSW FOR INTERRUPT

                RSEG    ?XD?TIGERD

INBUF:          DS      INSIZE          ;UART INPUT BUFFER
BUFEND          EQU     $               ;END OF BUFFER
XINBUF:         DS      XINSIZE         ;EXTERNAL INPUT BUFFER
XBUFEND         EQU     $               ;END OF BUFFER

                RSEG    ?XD?MEMSTART

MEMSIZE:        DS      XLEN

                RSEG    ?STACK

                DS      1

                XSEG    AT      0

TIMEPO:         DS      1                       ;TIMER POINTER
TIMERS:         DS      TIMENUM                 ;ABSOLUTE POSITIONNING OF
FASTPO:         DS      1                       ;FAST TIMER POINTER
FASTIMERS:      DS      FASTNUM                 ;TIMER FOR CLOCK HANDLER
DISPO:          DS      1                       ;LCD DISPLAY POINTER
DISBUF:         DS      6                       ;LCD DISPLAY BUFFER
DISFLAG:        DS      8                       ;LCD FLAGS BUFFER
DISLED:         DS      1                       ;DISPLAY LED BYTE
DISCOM:         DS      1                       ;DISPLAY CONTROLLER COMMAND
RELBUF:         DS      10                      ;RELCON BIT BUFFER

                XSEG    AT      PPAGE*100H

XUARTD:         DS      1
XUARTC:         DS      1
PORTA:
PORTB:          DS      1
;
;================================================================
;
                END