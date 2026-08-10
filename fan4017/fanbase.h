#define     vernum      11994       // sofware version number
// change in startup modul in dummy function ident

#pragma  debug
#pragma  symbols
#pragma  small
#pragma  nointpromote
#pragma  nointvector
#pragma  objectextend
#pragma  noprint

#include <reg52.h>
#include <intrins.h>
#include <absacc.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mydef.h>

#ifdef  ext
    #define  flag(name, loc, bitnum) sbit name = loc ^ bitnum
#else
    #define  ext  extern
    #define  flag(name, loc, bitnum)  extern bit name
#endif

#define  insize  32   // UART input buffer size
#define  lcdsize  6   // LCD display size
#define  novsize  16  // NOVRAM pages used (1 page = 8 bytes)
#define  refsig  0X9365    // NOVRAM reference signature
#define  comstat() (rsipo!=rsmpo)  // UART port input status
#define  flush()  rsmpo=rsipo   // Flushes the UART input buffer
#define  kbhit()  fkey    // Borland compatibility
#define  opstop() ((rsipo!=rsmpo)||fkey) // keyboard or RS232 activated
#define  examine(x) binstring((uchar *) &x, sizeof(x))

/*******        HARDWARE BITS           *******/

#define  SCL   T0   // I2C clock
#define  SDA   INT0  // I2C data
#define  PWR   T1   // Power supply, active low

sbit  zkey   = P1 ^ 0;   // ZERO key, active low
sbit  tkey   = P1 ^ 1;   // TARE key, active low
sbit  pkey   = P1 ^ 2;   // PRINT key, active low
sbit  blon   = P1 ^ 3;   // Backlignt, active low
sbit  bcd420 = P1 ^ 4;   // BCD/4-20 serial bit
sbit  calen  = P1 ^ 5;   // Calibration enable jumper

/*******     TIMING CONSTANTS   *******/

#define  hafsec  6   // time delay for timers[]
#define  onesec  11
#define  twosec  20
#define  trisec  30
#define  forsec  40
#define  tensec  100
#define  fulsec  255   // 25.5 seconds

/*******     I2C BUS CONSTANTS  *******/

#define  novrad  0XA0   // NOVRAM I2C address
#define  dispad  0X70   // Display I2C address
#define  rtcad   0XD0   // RTC I2C address
#define  adbase  0X20   // A/D I2C base address
#define  adfix   0X10   // Single device A/D sampling mode
#define  adrun   0X20   // A/D running mode
#define  dispen  0X40   // LCD display enable mode
#define  iccom   19     // Highest I2C command number

/*******        A/D COMMANDS    *******/

#define  adwey   1   // weighing mode
#define  adraw   2   // raw data mode
#define  adbat   3   // temperature mode
#define  adver   4   // software version
#define  adzer   5   // ZERO command
#define  adfac   6   // factor setup
#define  adref   7   // zero-reference & offset setup
#define  adful   8   // full scale and rounding
#define  adpar   9   // control parameters
#define  adran   10  // double range and options
#define  adlim   11  // ZERO command limits (OIML)
#define  zcal    12  // ZERO calibration command
#define  wcal    13  // weight calibration command
#define  tcal    14  // TCO calibration command
#define  hitco   15  // High TCO coefficients
#define  lotco   16  // Low TCO coefficients
#define  adset   17  // reset slave

#define  adtar  18   // A/D tare functions
#define  adcnt  19   // counting functions

//   COUNTING SUBCOMMANDS FOR ADCNT

#define  Cweight  0X80  // puts the A/D in normal weighting mode
#define  Cfactor  0X81  // requests the counting factor
#define  Ccount   0X82  // puts the A/D in basic weighing mode
#define  Cfix     0X83  // puts the A/D in weighing mode with factor fixup
#define  Czero    0X84  // starts a counting zero calibration
#define  Csample  0X85  // starts a counting sample calibration

/*******        OPERATING CONSTANTS     *******/

#define  true   1
#define  false  0
#define     CFACBIAS    40960           // Factor bias for 4-20mA calibration
#define     COFFBIAS    8192            // Offset bias for 4-20mA calibration
#define     VFACBIAS    2048            // Factor bias for 0-5V calibration
#define     VOFFBIAS    2048            // Offset bias for 0-5V calibration

/*******        NOVRAM VARIABLES        *******/

// PAGE 0 - Main operating parameters
#define    novsig     inword(novbuf, 0) // NOVRAM signature
#define    disform    novbuf[2]         // display format
#define    rsform     novbuf[3]         // RS232 format
#define    pcount     inword(novbuf, 4) // programming counter
#define    kcount     inword(novbuf, 6) // calibration counter

// PAGE 1 - Device 0 parameters
#define    fac0       inlong(novbuf, 8)  // factor
#define    zref0      inlong(novbuf, 12) // zero reference
#define    offset0    inbyte(novbuf, 12) // Offset level

// PAGE 2 - Device 0 parameters
#define    full0      inlong(novbuf, 16) // full scale
#define    round0     inbyte(novbuf, 16) // rounding unit
#define    param0     inlong(novbuf, 20) // working parameters
#define    filter0    inbyte(novbuf, 20) // sampling rate and filter mode
#define    stable0    inbyte(novbuf, 21) // stability criterion
#define    btemp0     inbyte(novbuf, 22) // base temperature for TCO
#define    auto0      inbyte(novbuf, 23) // autozero timing if negative

// PAGE 3 - Device 0 parameters
#define    track0     inlong(novbuf, 24) // options and tracking range
#define    drange0    inbyte(novbuf, 24) // double range limit (%)
#define    zrange0    inbyte(novbuf, 25) // zero tracking range (DIV/2)
#define    ztime0     inbyte(novbuf, 26) // TFORCE
#define    op0        inbyte(novbuf, 27) // option bits
#define    zerlim0    inlong(novbuf, 28) // zero limits (OIML)
#define    hiabs0     inbyte(novbuf, 28) // upper absolute zero limit
#define    loabs0     inbyte(novbuf, 29) // lower absolute zero limit
#define    hirel0     inbyte(novbuf, 30) // upper relative zero limit
#define    lorel0     inbyte(novbuf, 31) // lower relative zero limit

// PAGE 4 - Device 0 parameters
#define    target0    inlong(novbuf, 32) // Zero calibration target
#define    zmode0     inbyte(novbuf, 32) // Zero calibration mode
#define    load0      inlong(novbuf, 36) // Calibration load
#define    ctemp0     inbyte(novbuf, 36) // previous temperature for TCO

// PAGE 5 - Device 0 parameters
#define    hitco0     inlong(novbuf, 40) // high temperature TCO
#define    lotco0     inlong(novbuf, 44) // low temperature TCO

//  PAGE 6 - Device 0 parameters
#define    novop      inlong(novbuf, 48)  // Option1 (4 bytes 32 option)
#define    novop2     inlong(novbuf, 52)  // Option2 (4 bytes 32 option)

//  PAGE 7 - Device 0 parameters
#define    opset      novbuf[56]          // desired opset number
#define    poff       novbuf[57]          // power off
#define    mforce     inword(novbuf, 58)  // force zero range (2 bytes)
#define    kunit      novbuf+60           // unit weight (4 bytes)

//  PAGE 8 - Device 0 parameters
#define    backl0     inword(novbuf, 64)  // Back light timer in minutes
#define    stop0      inlong(novbuf, 66)  // STOP (4 bytes 32 option)
#define    zref1      inlong(novbuf, 70)  // zero reference (4 bytes)

//  PAGE 9 - Device 0 parameters
#define    full1      inlong(novbuf, 74)  // full (to calculat factor only) (4 bytes)
#define    bcdisp     novbuf[78]          // bcd display part
#define    zref2      inlong(novbuf, 79)  // zero reference no change only in clibration

//  PAGE 10 - Device 0 parameters
#define    batisp     novbuf[83]          // bcd display part
#define    dtime      novbuf[84]          // delay between rel's
#define    dtime8     novbuf[85]          // delay befor rel 8 on
#define    spmax      novbuf[86]          // max number of setpoint
#define    zerodiv    inword(novbuf, 87)  // positive zero range for relayZ(ero)
#define    zerop      inword(novbuf, 89)  // 


//  PAGE 11 - Device 0 parameters
#define    spdata0    inlong(novbuf, 91)  // 
#define    spdata1    inlong(novbuf, 95)  // 

//  PAGE 12 - Device 0 parameters
#define    spdata2    inlong(novbuf, 99)  // 
#define    spdata3    inlong(novbuf, 103)  // 

//  PAGE 13 - Device 0 parameters
#define    spdata4    inlong(novbuf, 107)  // 
#define    spdata5    inlong(novbuf, 111)  // 

//  PAGE 14 - Device 0 parameters
#define    tforce0    novbuf[115]          // time zero force
#define    line0      novbuf[116]          // Label height in LF units
#define    tear       novbuf[117]          // Value of adv/rev LF for 'tear-off' facility
#define    profiID    inword(novbuf, 118)  // PROFI ID number
#define    profiadd   novbuf[120]          // profibus address   
#define    net        novbuf[121]          // scale net number

#define    pass       inword(novbuf, 122)  // password for setpoint
#define    sn         inlong(novbuf, 124)  //  serial number
/*******        FUNCTION PROTOTYPES     *******/

//  AFUN.ASM
void    watchdog(void);
uchar   nada(uchar);
void    reboot(void);
uint    biostime(uint);
bit     btm(uint);
void    delay(uint, uchar);
bit     icout(uchar);
bit     adout(uchar);
uchar   icin(uchar);
void    icstop(void);
void    clrscr(void);
uchar   getch(uchar);  // IF GETCH(1) WITH COMUNICATION INTERAPT 
uchar   setbit(uchar);
uchar   getbit(uchar);

//  WEIGHT.C
bit     status(uchar);
ulong   getzer(uchar, uchar);
ulong   getare(uchar, ulong);   // tare function controlled by the tare high byte
ulong   tareweight(uchar, float);  // set tare to a given weight
uint    fan2gul(long n);
long    gul2fan(uint n);
ulong   wtare(ulong t);

//  PROGMODE.C
void    progmode(void);

//  TESTMODE.C
void    testmode(void);
float   fac2float(ulong);
ulong   float2fac(float);
ulong   getver(uchar);
void    testkey(uchar);
void    atod2weight(long avg, uchar n);

//  CALIB.C
void    calib(void);
void    home(void);
void    gosave(void);
void    psave(void);
void    errdisp(uchar);
void    errors(uchar);

//  STARTUP.C
uchar   startup(void);
uchar   wakeup(void);
void    dummy(void);

//  UART.C
void    comini(uchar);
uchar   getcom(void);
void    comstring(uchar *);
void    binstring(uchar *, uchar);
uchar   pontype(uchar);

//  NOVRAM.C
bit     novread(uchar *, uchar);
bit     novwrite(uchar *, uchar);
bit     novload(void);
void    novsave(void);

//      RTC.C
bit     clockin(uchar *, uchar);
bit     clockout(uchar, uchar *);
void    datime   (void) ;       // f01 - set date & time
uchar   gettime  (void) ;
uchar   settime  (void) ;
uint    controlDate(void) ;
uint    controlTime(void) ;
void    set_time_end_date();
void    sscanf_date(uchar *date) ;
void    sscanf_time(uchar *time) ;

//  ATOD.C
bit     icini(void);
void    icset(uchar);
bit     adcom(uchar, uchar);
bit     adload(uchar);
uchar   adini(void);
bit     atod(uchar);
void    refresh(uchar); 

//  CONSOLE.C
void    display(long, uchar);
void    keyboard(uchar);
void    altern(uchar *, ulong, uchar);
ulong   getdata(uchar *, ulong, uchar, ulong);
bit     cdouble(uchar);
void    pause(uchar);
uint    getvalue(uchar *, uint , uchar, ulong);
void    blink(uchar *, uchar * , uchar );
ulong   ConvertLongByte(uchar *);
bit     xflag(ulong x, uchar n);
void    discon(uchar *);

//  SERIAL.C
void    serial(void);     // debugging functions
void    SendWeightToCom(ulong b);
float   fpow(float b, int e); // expanenta calculation
uchar   DecimalPointNum(uchar k);
void    SendIdent();
void    SendZeroReq(uchar i);
void    xputs();

void    sendweight()    ;
void    sendidentifier();
void    sendatod()      ;
void    sendfullscale() ;
void    sendtarevale()  ;
void    resersacle()    ;
void    sendzero()      ;
void    sendtare()      ;
void    sendaddress()   ;
void    sendprofiID()   ;
void    senderror(uchar);
void    senddecimalpoint() ;

void 	comuniwell(uchar k);

//  RELSYS.C
void    send_rel(uint imrel);
void    getkey();
void    setpoint();
void    define_sp(void);
void    manual_sp();
void    rel_function() ; 

//  POWER.C
void    pwroff(void);     // power supply switch off
uint    getbat(uchar);     // battery level check
void    power_off();
void    power_off_reset();
void    check_bat(uint);

//  PRINT.C
void    Print(uchar);
void    eol();
void    lprintf(uchar *);
void    clear_all(void);
void    subdisp(void);
void    pdone(void);
void    Total();
void    subwgt(void);

//  MAIN.C
void    battery(void);
uint    bcd(ulong, uchar) ;
void    remove_weight();

//  NET.C
void    wttrans(void);
void    ttrans(void);
void    network(uchar);
void    answer(uchar *b);
uchar   ncs(uchar *s);

//  COUNTING.C
ulong   getfac(uchar);     // reading the counting factor
bit     countmode(uchar, uchar);   // counting mode: 0=weighing, 1=counting; 2=counting+fixup
bit     countfac(uchar, ulong);   // counting mode (1) with a forced counting factor
bit     countunit(uchar, float);  // counting mode (1) with a forced unit weight (grams)
ulong   getcount(uchar, ulong);   // counting sample calibration
void    UnitWeight();
float   oneunit(ulong unit);
void    Count();

//  CAL420.C                        // TRAN420 converter
void    com420(uint);               // 4-20mA command
void    calib420(void);             // 4-20mA keyboard calibration      Function 11
ulong   param420(uint, uint);       // 4-20mA calibration parameter computation
void    calib05(void);              // 0-5V keyboard calibration        Function 12
ulong   param05(uint, uint);        // 0-5V calibration parameter computation

/*******        ABSOLUTE VARIABLES      *******/

extern data uchar  rsipo;     // UART ISR input pointer
extern data uchar  rsmpo;     // UART main input pointer
extern data uchar  rsoco;     // UART output byte counter
extern data uchar  rsmode;    // UART output mode (RAM/CODE)
extern data uint   rsopo;     // UART output pointer
extern data uint   biostick;  // BIOS tick running at 2400/sec
extern data uint   bcdbuf;    // BCD and 4/20mA buffer
extern data uchar  adsamp;    // A/D sampling mode

extern pdata uchar bufin[];   // UART input buffer
extern pdata uchar timers[];  // 60 .1sec timers
extern pdata ulong adbuf[];   // A/D received weight
extern pdata uchar adtype[];  // A/D information type
extern pdata uchar adstat[];  // A/D status byte
extern pdata uchar lcdbuf[];  // LCD display buffer
extern pdata uchar disbuf[];  // Display ASCII buffer
extern pdata ulong brut, atodbrut;      // brut A/D output
extern pdata funptr fun;      // calibration/test/program pointer
extern pdata uchar rsbyte;    // avoiding getcom recursion

/*******     BIT ADDRESSABLE VARIABLES  *******/

ext  bdata  uint   visual;    // LCD visual indicators
ext  bdata  uchar  scancode;  // Keyboard scan code
ext  bdata  uchar  icmode;    // I2C working mode
ext  bdata  ulong  option;    // option bits (31 option max)
ext  bdata  ulong  option2;   // option bits (31 option max)

#define opnum 57              // option number (64 option max)
 
/* 1  */flag(okilo   , option , 0 );    // Kg indicator ON
/* 2  */flag(opound  , option , 1 );    // Lb indicator ON
/* 3  */flag(ostab   , option , 2 );    // Kg/Lb used as stability indicators
/* 4  */flag(onostab , option , 3 );    // No in printing (1) / wait to stable in printing
/* 5  */flag(onotitle, option , 4 );    // No titles in print
/* 6  */flag(ophold  , option , 5 );    // hold forzen display
/* 7  */flag(ofult   , option , 6 );    // Full scale tare range(1) or 5% of 'wstop'(0)
/* 8  */flag(ocumt   , option , 7 );    // Cumulative(1) or toggle(0) tare
/* 9  */flag(odisp   , option , 8 );    // Dinamic(1)/stable(0) display
/* 10 */flag(opgetze , option , 9 );    // get zero from memory
/* 11 */flag(opcs    , option , 10);    // check sum send in wiz protocol
/* 12 */flag(oconti  , option , 11);    // Weight continuous transmition/Manual transmition
/* 13 */flag(otrans  , option , 12);    // Transmission to computer(1) or printer(0)
/* 14 */flag(oautom  , option , 13);    // Automatic printing on stabel
/* 15 */flag(ogross  , option , 14);    // Gross weight (brutto) print
/* 16 */flag(otare   , option , 15);    // Tare print
/* 17 */flag(ototal  , option , 16);    // Subtotal print
/* 18 */flag(otonge  , option , 17);    // Hebrew(1)/english(0) tongue
/* 19 */flag(ohalf   , option , 18);    // Print new weight after decreasing to half of previous
/* 20 */flag(opwiz   , option , 19);    // wiz protocol
/* 21 */flag(opseg   , option , 20);    // cancel "+" or "-" in weight transmit 
/* 22 */flag(opadi   , option , 21);    // ADI PROTOCOL
/* 23 */flag(opbat   , option , 22);    // battry conected
/* 24 */flag(bcdini  , option , 23);    // BCD status for ini, neg & err cases: 0(on)/-1(off)
/* 25 */flag(bcdpol  , option , 24);    // BCD polarity (on: 1-high, 0-low / off: 1-low, 0-high)
/* 26 */flag(ocount  , option , 25);    // Counting mode enable
/* 27 */flag(oldheb  , option , 26);    // New(1)/old(0) hebrew print table
/* 28 */flag(opbatdisp, option, 27);    // "BAT" DISPLAT AT THE STARTUP 
/* 29 */flag(omant   , option , 28);    // Enable(1) or disable(0) manual tare
/* 30 */flag(opbigdsp, option , 29);    // Big Display
/* 31 */flag(ospace  , option , 30);    // Weight transmit with space(1)/'0'(0) lead characters
/* 32 */flag(oastrk  , option , 31);    // Weight transmit with * lead character 
/* 33 */flag(oetare  , option2, 0 );    // Enable(0) or disable(1) tare
/* 34 */flag(oneg    , option2, 1 );    // No displayed negative weight
/* 35 */flag(otoneg  , option2, 2 );    // Auto-zero of negative weight (oneg must be set)
                                        // zero if weight is under atod 0 (0) / not zero (1)
/* 36 */flag(opbcd   , option2, 3 );    // communication = 4 - 20 (0) / BCD (1)
/* 37 */flag(bcdsel  , option2, 4 );    // BCD bisign(on)/unsign(off) selection
/* 38 */flag(ospord  , option2, 5 );    // the next setpoint turn off the previous
/* 39 */flag(osetpoint,option2, 6 );    // SETPOINT COMMUNICATION.
/* 40 */flag(osubrel8, option2, 7 );    // WAIT FOR STABEL AND TOTALIZING BEFOR REL 8 
/* 41 */flag(olabel  , option2, 8 );    // Label(1)/line(0) print format
/* 42 */flag(otear   , option2, 9 );    // Advance the paper to tear off position
/* 43 */flag(olfeed  , option2, 10);    // CR without LF(1 - for CITIZEN) or without LF(0)
/* 44 */flag(ototnz  , option2, 11);    // Subtotal not zeroed after print (1)
/* 45 */flag(ollabel , option2, 12);    // Label Format

/* 46 */flag(oprofi  , option2, 13);    // profi protocol
/* 47 */flag(oldprofi, option2, 14);    // old profi (0) / new profi (1) bit reset send
/* 48 */flag(odfrofi , option2, 15);    // SEND WEIGHT + STATUS TO PROFI (0) / SEND WEIGHT ONLY TO PROFI (1)
/* 49 */flag(onet    , option2, 16);    // net protocol
/* 50 */flag(opone   , option2, 17);    // remove zero befor transmit 00.000 one protocol set 18
/* 51 */flag(opdcount, option2, 18);    // SHOW "111111" TO "999999" IN STARTUP
/* 52 */flag(opvol   , option2, 19);    // voltage mode for 4-20mA (0) / 0-5 V (1) communication (TRAN420)
/* 53 */flag(onostabTZ, option2, 20);   // no stable befor make zero or tare
/* 54 */flag(opcasio , option2, 21);    // casio uniwell protocol
/* 55 */flag(optare  , option2, 22);    // make tare when press start - minirel
/* 56 */flag(opPrint , option2, 23);    // print new line in evrey weight
/* 57 */flag(opprints, option2, 24);    // add stable flag to weight in communication
/* 58 */flag(op58    , option2, 25);    //
/* 59 */flag(op59    , option2, 26);    //
/* 60 */flag(op60    , option2, 27);    //
/* 61 */flag(op61    , option2, 28);    //
/* 62 */flag(op62    , option2, 29);    //
/* 63 */flag(op63    , option2, 30);    //
/* 64 */flag(op64    , option2, 31);    //


// Visual indicators
flag(dlr  , visual, 2);    // lower right arrow
flag(dmr  , visual, 3);    // middle right arrow
flag(dur  , visual, 4);    // upper right arrow
flag(dpcs , visual, 5);    // "PCS" indicator for counting
flag(dkg  , visual, 6);    // "KG" indicator
flag(dlb  , visual, 7);    // "LB" indicator
flag(dll  , visual, 10);   // lower left arrow
flag(dml  , visual, 11);   // middle left arrow
flag(dul  , visual, 12);   // upper left arrow
flag(dbat , visual, 13);   // "BAT" indicator
flag(dtare, visual, 14);   // "TARE" indicator
flag(dzero, visual, 15);   // "ZERO" indicator

 
// Unused visual bits for UART
flag(fbit , visual, 0);    // UART 8 bits
flag(fpen , visual, 1);    // UART parity enable
flag(fodd , visual, 8);    // UART odd parity
flag(fsend, visual, 9);    // UART transmitter busy.

// Keyboard bits
flag(kzero , scancode, 0);   // ZERO key
flag(ktare , scancode, 1);   // TARE key
flag(kprint, scancode, 2);   // PRINT key
flag(fkey  , scancode, 3);   // Keyboard hit flag

// Unused scancode bits 4..7
flag(fscroll, scancode, 4);   // display scroll flag
flag(fsync  , scancode, 5);   // I2C bus under interrupt control
flag(fstop  , scancode, 6);   // I2C bus request
flag(fpower , scancode,7);    // power off enabled

// I2C mode bits
flag(flcd, icmode, 7);    // I2C enable bit
flag(frun, icmode, 6);    // A/D run mode

/*******        BIT VARIABLES           *******/
ext  bit  frame;      // LCD frame transmitted
ext  bit  fbcd;       // BCD transmission enable
ext  bit  opol;
ext  bit  ferror;     // general error flag
ext  bit  fdisp;      // display data every .1 second
ext  bit  falter;     // alternated messages flag
ext  bit  fnov;       // unprogrammed NOVRAM
ext  bit  fprog;      // programmed parameter modified
ext  bit  fbat1, freqw;
ext  bit  fresh;     // refresh done
ext  bit  fhold;     // frozen display  
ext  bit  fstatus;

ext  xdata   uchar  ftkeyp, funder0;
ext  xdata   uchar  fzng, fsave, fngz, fone;
ext  xdata   uchar  fstab;
ext  xdata   uchar  fover;
ext  xdata   uchar  funder,fbat;
ext  xdata   uchar  fzero;
ext  xdata   uchar  fnew;              // new weight return to zero
ext  xdata   uchar  ftitle;            // print title
ext  xdata   uchar  ftzero;            // computer request zero from scale
ext  xdata   uchar  fdsneg;

/*******        BYTE VARIABLES          *******/
ext  pdata  uchar  repbuf[16];  // RS232 report buffer

ext  data   uchar  cursor;       // display cursor
ext  data   uchar  errnum;       // A/D error number
ext  data   uchar  prog;         // Programming mode

ext  code   uchar  ack1[];       // acknowledge
ext  code   uchar  nak1[];       // noacknowledge

ext  code   uchar  novdef[];     // NOVRAM default values
ext  code   uchar  opdef [];     // OPTION default values
ext  code   uchar  novdef[];     // NOVRAM default values
ext  code   uchar  *bformat[];   // External UART baud rate display string
ext  code   uchar  *uformat[];   // UART communication format

ext  xdata  uchar  adnum;       // number of detected A/D devices
ext  xdata  uchar  novbuf[novsize << 3]; // NOVRAM buffer
ext  xdata  uchar  ascbuf[80];           // genral purpose buffer
ext  xdata  uchar  secbuf[80];           // secend string buffer
ext  xdata  uchar  num, zerind;
ext  xdata  uchar  fmerror, fStartProfi;
/*******        INT VARIABLES           *******/

ext  code   uint  ubaud[];  // UART baud rate decoder

ext  xdata  uint  tall, oldbat, backl;
ext  xdata  uint  poffcounter;            // power off counter
ext  xdata  uint  order;                  // weight number

/*******        LONG VARIABLES          ******/

ext  xdata  ulong  progvar;   // programming variable
ext  xdata  ulong  tare;        // tare
ext  xdata  ulong  weight, oldweight;
ext  xdata  ulong  wstop, weight0;
ext  xdata  ulong  netto;       // netto print
ext  xdata  ulong  ptotal;      // netto total
ext  xdata  ulong  gtotal;      // grund total
ext  xdata  ulong  is_battery ; // is battery present (password = 1955)
ext  xdata  ulong  zref, temp, szero, tempzero;
ext  xdata  ulong  raw, olddisp;

/*******        FLOAT VARIABLES         *******/

ext  xdata float facdisp;  // factor to be displayed

/*******        TIMER DEFINITIONS       *******/

#define  timout   timers[0]       // general purpose timer
#define  talter   timers[1]       // alternated message timer
#define  twait    timers[2]       //
#define  tblink   timers[3]       //
#define  timeout  timers[4]       //
#define  tbl      timers[5]       // back  light timer
#define  treqz    timers[6]       // zero  timer reqwest
#define  treqt    timers[7]       // tare  timer reqwest
#define  treqp    timers[8]       // print timer reqwest
#define  tpoff    timers[9]       // power off
#define  tforce   timers[10]      // force zero
#define  tref     timers[11]      //
#define  tstab    timers[12]      //
#define  tbat     timers[13]      //
#define  tatod    timers[14]      //
#define  terrin   timers[15]      // error display timer
#define  terrout  timers[16]      // error recovery timer
#define  tprint   timers[17]      //
#define  tneway   timers[18]      // rs232 reciever timeout for new relbox
#define  trel8off timers[19]      // relay 8 off
#define  tdelay   timers[20]      // delay for relcon between setpoints
#define  tdsneg   timers[21]      // blink '-' or weight if weiget morte then 100000
#define  trsnet   timers[22]      // rs232 transmit timeout (network)
#define  tdisplay timers[23]      // display refreshing
#define  tsend    timers[24]      // Display SEND

ext  xdata  struct  date  {
                    uint  da_year ;
                    uchar da_day  ;
                    uchar da_mon  ;
                    uchar da_week ; /* day of week */
}     sdate   ;

ext  xdata  struct  time  {
                    uchar ti_min  ;
                    uchar ti_hour ;
                    uchar ti_hund ;
                    uchar ti_sec  ;
}     stime   ;


/*******************************    SetPoint   ****************************/
#define spnum 6

ext  xdata  uchar  fstart, bytest ;    // start flag for new relbox
ext  xdata  uchar  keystatus ;         // relbox keys status
ext  xdata  uchar  flock, frel8off, fdelay1, frelsub8, indw;
ext  xdata  uchar  ftest, fpatod, fmakez, ftare;

ext  xdata  uint   sprel, spnew;

ext  xdata  ulong  spdata[spnum], neww;

/***************************** Net Protocol *****************************/

#define nettareval  'V'
#define netweight   'W'
#define nettare     'T'
#define netzero     'Z'

#define exitfun     fullsec;

#define request     netstring[0]
#define savuest     netstring[4]

ext  xdata  uchar   await;
ext  xdata  uchar   netcs ;         // net request checksum
ext  xdata  uchar   netstring[5] ;  // net scale number
ext  xdata  uchar   netbuf[160] ;   // net buffer (for sp/cw command string receiption
ext  xdata  uchar   counter ;       // command string bytes counter
ext  xdata  uchar   phext ;         // network's phase next step
ext  xdata  uchar   ftranstare, ftranzero, freboot;

ext  xdata  ulong  t1,z1;
