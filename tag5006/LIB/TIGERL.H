 
#define isize      8            // I2C buffer size for data transfer
#define xmode      0X66;        // External UART main control
#define xout       0X46;        // External UART transmit disable
#define lf         10
#define ff         12
#define cr         13
#define off        0
#define on         1
#define false      0
#define true       1
#define test       2
#define both       0            // trim function: cut from the both sides
#define left       1            //                    from the left side
#define right      2            //                    from the right side

#define sda       T0            // I2C sda
sbit adata      = P1 ^ 0;       // A/D serial input (inverted)
sbit asel       = P1 ^ 1;       // A/D and TRIMMER chip select (active high)
sbit acal       = P1 ^ 2;       // A/D calibration (high) and TRIMMER INC
sbit scl        = P1 ^ 3;       // A/D clock, TRIMMER U/D, I2C scl, WD
sbit kbdbit     = P1 ^ 6;       // Serial keyboard (input)
sbit spout      = P1 ^ 7;       // current loop output (active low)
//sbit    lcdrs   = P1 ^ 5;       // LCD R/S select bit
#define lcden     T1            // LCD enable bit

#ifdef ext
#define op(name, number) sbit name = options[(number-1)>>3]^((number-1)&7)
#define hop(name, number) sbit name = hidop[(number-1)>>3]^((number-1)&7)
#else
#define op(name, number) extern bit name
#define hop(name, number) extern bit name
#define ext extern
#endif
#ifndef exb
#define exb extern
#endif

/* AFUN.ASM */
uchar   nada(uchar);
void    watchdog(void);
void    bell(uchar);
uchar   flag(uchar, uchar);
void    clrscr(void);
bit     kbhit(void);
uint    atodin(void);
void    recycle(void);
void    relset(uint);
uchar   mirror(uchar);
/* ATOD.C */
void    wait(uchar);
bit     trimset(uchar) small;
bit     calib(void);
uchar   trimfix(uint);

/* CLOCK.C */
#define  rtcad      0XD0         // RTC I2C bus address
bit     clockin(uchar *, uchar) small;
bit     clockout(uchar, uchar *) small;

/* ICBUS.C */
void    icini(void);
bit     open(uchar, uchar);
void    close(void);
bit     write(uchar) small;
bit     read(uchar, uchar) small;

/* KBD.C */
uchar   getch(void);
void    ungetch(void);
uchar   keypress(void);
void    switchoff(void);

/* NOVRAM.C */
#define  novad      0XA0         // NOVRAM I2C bus address
bit     novin(uchar *, uchar) small;
bit     novout(uchar, uchar *) small;

/* TOOLS.C */
void    tools(void) ;                   // dummy function
void    pause(uchar time) ;
bit     getbitport(uchar n) ;
void    message(uchar *, uchar) ;
float   max(float, float) ;
float   min(float, float) ;
void    strspace(uchar *s, int n) ;
bit     is_space(uchar *s) ;
uchar  *trim(uchar *s, uchar side) ;
float   fpow(float b, int e) ;          // expanenta calculation
uchar  *hebrew(uchar* d, uchar *s) ;
void   *memlove(uchar *p2, uchar *p1, uint t) ;

/* UART.C */
void    comini(uchar) small;
void    flush(void) small;
bit     comstat(void) small;
uchar   getcom(void) small;
void    putcom(uchar) small;
void    comeol(void) small;
void    comstring(uchar *) small;
void    binstring(uchar *, uint) small;
void    xcomini(uchar) small;
void    xflush(void) small;
bit     xcomstat(void) small;
uchar   xgetcom(void) small;
void    xputcom(uchar) small;
void    xcomeol(void) small;
void    xcomstring(uchar *) small;
void    xbinstring(uchar *, uint) small;
uchar   allstat(void) small;
uint    allcom(void) small;


/* LCD.C */
void    lcdtran(uchar) small;
uchar   lcdbit8(uchar, uchar) small;
uchar   lcdbit4(uchar, uchar) small;
void    lcdini(uchar) small;
void    lcdstart(void);
void    chargen(uchar c, uchar bmp[]) small;
uchar  *htrans(uchar *s);
uchar  *htrans_new(uchar *s);
void    lcdputs(uchar x, uchar y, uchar *s);


/* * *  EXTERNAL VARIABLES, PROVIDED BY THE MAIN PROGRAM  * * */

extern data  uchar      beeptime, skey, pkey, rsmode;
extern data  uint       ucount, receive, transmit;
extern data  uint       xcount, xreceive, xtransmit;
extern pdata uchar      porta, portb, xuartd, xuartc;
extern xdata uchar      timers[], fastimers[], relbuf[];
extern xdata uchar      inbuf[], bufend[], xinbuf[], xbufend[];
extern xdata uchar      disbuf[], disflag[], discom, disled;

extern code uchar       lcdtab[];   // LCD 7 segments encoding table
extern code uchar       magtab[];   // MAG 7 segments encoding table
extern code uchar       keylist[];  // Keyboard scancode decoding array
extern code uchar       ubaud[];    // Internal UART baud rate decoder
extern code uchar       xbaud[];    // External UART baud rate decoder
extern bit  obusy;                  // Option for busy line
extern bit  ocheck;                 // Check-weight flag

ext    bit    flcdon;               // LCD initialization request
ext    bit    flcdgo;               // LCD initialization in course
ext    bit    fsamp;                // A/D sampling request
ext    bit    fkey;                 // keyboard hit flag
ext    bit    fsend;                // Internal UART busy sending flag
ext    bit    fxsend;               // External UART busy sending flag
ext    bit    fbit;                 // Internal UART 8 bits mode
ext    bit    fdis;                 // Internal UART parity disable
ext    bit    fodd;                 // Internal UART odd polarity
ext    bit    fscroll;              // Display scrolling flag

#if isize>16
ext    xdata  uchar     icbuf[isize];
#else
ext    xdata  uchar     icbuf[16];
#endif

ext    bdata  uchar     bimage;
ext    data   uchar     disreg, cursor, scode, pcode, scancode;
ext    xdata  uint      uartin, xuartin, cyclad;
