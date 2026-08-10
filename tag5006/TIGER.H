
/* * * * * * * *  S O F T W A R E   I D  * * * * * */
 
#define prog     1      // Programmer id (itzik)
#define year     0      // Last year digit  28-06-2001
#define month    0      // Month
#define day      00      // Day of the month

#define ident    11382
/* * * * * * * * * * * * * * * * * *  * * * * * */
 
#pragma      debug
#pragma      symbols
#pragma      large
#pragma      nointpromote
#pragma      nointvector
#pragma      objectextend
#pragma      noprint
#include     <reg51.h>

#define uchar unsigned char
#define uint  unsigned int
#define ulong unsigned long

#include     "lib\tigerl.h"                 // library files

#include <intrins.h>
#include <absacc.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* * * * * * * *  DISPLAY FLAGS  * * * * * * */

#define fupl     0      // upper left corner
#define ftare    1      // "TARE" indicator
#define fzero    2      // "ZERO" indicator
#define flol     3      // lower left corner
#define flo      4      // "LO-" indicator
#define fbat     5      // "BAT" indicator
#define fmidl    6      // middle left arrow
#define fmidr    7      // middle right arrow
#define foz      8      // "-OZ" indicator
#define fpound   9      // "LB" indicator
#define fupr    10      // upper right corner
#define fkilo   11      // "KG" indicator
#define fpcs    12      // "PCS" indicator
#define flor    13      // lower right corner

/* * * * * *  LIGHT INDICATORS (MEIRAV 3001)  * * * * */

#define none      0
#define yellow    2
#define green     4
#define red       8
#define relayZ     7            // number of ZERO relay

/* * * * * *  OPERATING CONSTANTS  * * * * */

#define bdef       0X40         // PORTB default at power up
#define offlim     2200         // Upper limit for A/D offset
#define a2dlim     500          // atod permissible offset from hiatod
#define decsec     1
#define quasec     4
#define hafsec     6
#define onesec     10
#define twosec     20
#define trisec     30
#define tensec     100
#define t24sec     240
#define fullsec    255
#define twomin     120
#define divnum     3000         // Scale resolution (3000 divisions)

/* * * * * * * 0.1 SECOND TIMERS (UP TO 32) */

#define trefresh    timers[0]           // display refresh timer
#define timeout     timers[1]           // general purpose timeout
#define tblink      timers[2]           // pattern timer
#define tstab       timers[3]           // stability timer
#define treqz       timers[4]           // Z request timer
#define treqt       timers[5]           // T request timer
#define ttrack      timers[6]           // tracking  timer
#define toff        timers[7]           // auto turn off timer (after 4 min)...
#define tforce      timers[8]           // force zero timer
#define tlobat      timers[9]           // lo-bat blink timer

#define tputs       timers[10]          // display freezing timer
#define tlife       timers[11]          // used only by longlife time of setpoints
#define tcount      timers[11]          // used only by countfactor definition
#define tdelay0     timers[12]          // delay for relcon in zero point
#define tdelay      timers[13]          // delay for relcon between setpoints
#define tdelay2     timers[14]          // twosec delay for stable weight transmit
#define twait       timers[15]          // expection general timer

#define trel8of     timers[16]           // relay 8 off
#define t2          timers[17]           // in use
#define tbacklight  timers[18]           //
#define t4          timers[19]           //in use
#define t5          timers[20]           //in use
#define tledoff     timers[21]           //
#define trelkbd     timers[22]           //
#define texitfun    timers[23]           //
#define trestart    timers[24]           //
#define tstart      timers[25]           //

#define trel3on     timers[26]           //
#define tlife1      timers[27]           //
#define tdelayz     timers[28]           //
#define t14         timers[29]           //
#define t15         timers[30]           //
#define t16         timers[31]           //
/* * * * * * * 3.33 MILLISECOND FAST TIMERS (UP TO 8) */

#define tfil        fastimers[0]        // filter coefficient ramp
#define tgostab     fastimers[1]        // requested stability time
#define trelay      fastimers[2]        // turn on the old relbox
#define tneway      fastimers[3]        // rs232 reciever timeout for new relbox
#define trs232      fastimers[4]        // rs232 transmit timeout
#define tdr         fastimers[5]        //
#define tonoff      fastimers[5]        //
#define timelcd     fastimers[6] // LCD PVC display time
#define trsnet      fastimers[7]    // rs232 transmit timeout (network)

/*                   MACRO DEFINITIONS                  */

#define nop             _nop_()
#define nop2            _nop_();_nop_()
#define nop3            _nop_();_nop_();_nop_()
#define nop4            _nop_();_nop_();_nop_();_nop_()
#define testbit         _testbit_
#define zeron           0x80000000      // zero negative for setpoints
#define byte3           0x00ffffff
#define byte(n, m)      *(((uchar *) (&n)) + m)
#define word(n, m)      *((uint *) (((uchar *) (&n)) + m))
#define quad(n, m)      *((ulong *) (((uchar *) (&n)) + m))
#define real(n, m)      *((float *) (((uchar *) (&n)) + m))
#define outportb(port,byte) port = byte
#define inportb(port,byte) byte = port
#define examine(x) binstring((uchar *) &x, sizeof(x))

extern  xdata uint   memsize;
extern  code  ulong  id ;          // software identifier
extern  code  uchar  novdef[];     // NOVRAM default values
extern  code  uchar  opdef[];      // NOVRAM default user options
extern  code  uchar  passcode[];   // Password array
extern  code  uchar *ubaudisp[];   // Internal UART baud rate display string
extern  code  uchar *xbaudisp[];   // External UART baud rate display string
extern  code  uchar *uformat[];    // UART communication format string
extern  code  float  mload[] ;     // Load value rounding
extern  code  uchar *namlist[],hipuch[];    // Product name table
extern  code  ulong  null[];       // null string
extern  code  uchar  crlf[];       // CRLF string
extern  code  uchar  ack[ ];       // acknowledge
extern  code  uchar  nak[ ];       // noacknowledge

/*
  ⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
  ≥       Main program modules                 ≥€
  ¿ƒ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹€
*/

/* TIGERS.C */
void    _main(void);
void    initxbits();
void    initvalue();
void    memtest() ;
uchar   checksum(uchar, uchar) ;

/* TIGERM.C */
void    battery(void);
void    process(void);
uint    dec4bcd(long);
bit     voltage(void);
void    blight(void);
void    xputs(uchar *s);

/* TIGERT.C */
void    testmode(void);
void    kdouble(uchar);
bit     cdouble(uchar);
void    hometest(void);
ulong   lloor(ulong w);
void    fun85(void);
void    fun86(void);
void    funcon(void);
void    fun88(void);

/* TIGERW.C */
void    weight(uint) small;
char    filter(ulong *, uint) small;
void    stable(char) small;
void    zerohandler(void) small;
void    forcezero(long);
void    manualtare(uint);
void    cumulatare(void);
void    toggletare(void);
long    factorize(long);
void    drift(void) small;
void    wlimit(void) small;

/* TIGERK.C */
uchar   keyboard(void);
void    testkeyt(void);
void    testkeyz(void);
void    testkeyp(void);
void    testkeyd(uchar);
void    display(long);
void    sformat(uchar *, long, uchar);
void    fformat(uchar *, float);
void    zformat(uchar *, uchar);
uchar   label(uchar *, uchar *);
ulong   getvalue(ulong, ulong, uint, uint) ;
float   getfvalue(uchar *, float, uchar, ulong) ;
ulong   undisp(uchar) small;
uchar   unput(uchar) small;
bit     rvalid(uint) ;

/* TIGERF.C */
void    wakeup(void);
void    execute(void (*f)());
void    light(uchar) small;
void    backlight(uchar) small;
void    drefresh(uchar) small;
uchar   password(void) small;
void    pattern(void) small;
void    setdiv(void);
void    errors(uchar, uchar);
void    stackok(void);
void    loadef(void);
bit     novsave(uchar);
bit     xbit(uchar n, uchar r);
void    dummy(void);
void    beep(int);

/* TIGERC.C */
void    checkweigher(long w) ;
void    fsetpoint(long w) ;     // filling setpoint
void    psetpoint(long w) ;     // pouring setpoint
void    relay1on(void) ;        // set relay number 1
uint    relbit(uchar n) ;       // set relay number n
void    relcom(uint, uchar) ;   // new relbox routine
void    defpour(void) ;         // define the fpour status

/* TIGERI.C */
void    toggle0  (void) ;       //   0 - weighing/counting toggle
void    toggle1  (void) ;       //   1 - manual/auto totalizing toggle
void    getcode  (void) ;       //   2 - code introducing
void    manual_bcd (void) ;     //   2 - manual bcd
void    define_cw(void) ;       //   3 - checkweigher
void    define_sp(void) ;       //   3 - setpoint
void    subdisp  (void) ;       //   4 - subtotal display
void    csample  (void) ;       //   5 - set count mode by sample
void    unitwgt  (void) ;       //   6 - set count mode by unit weight
void    membyte  (void) ;       //   7 - memory size in Kbytes
void    brutwgt  (void) ;       //   8 - current brute weight display
void    datime   (void) ;       // f01 - set date & time
uchar   gettime  (void) ;
uchar   settime  (void) ;
void    ranges   (void) ;       // f02 - set zero values range
void    ranset   (void) ;       // f03 - set low/high ranges for auto totalizing
void    getnam   (void) ;       // f04 - get product's name
void    delete   (void) ;       // f07 - erase code from 'mcode' area
void    smcode   (void) ;       // f09 - busy/free 'mcode' area status
void    splong   (void) ;       // f11 - setpoint's long time definition
void    relontimers(void);
void    _dload   (void) ;       // f12 - set the dead load zone
uint    cdelay(uchar *, uint, ulong) ; // f8, f10, f13 - set the dead time delay
void    manual_sp(void) ;       // f14 - setpoint's manual control
void    clear_tot(bit ) ;       // f98 - erase all sub [and grand] totals
void    clear_all(void) ;       // f99 - erase all codes
void    pdone    (void) ;
void    blink(uchar *s, uchar t) ;
void    splong1(void);

/* TIGERX.C */
bit     fexist(uchar *s) ;
bit     ferase(uchar *s) ;
bit     fwrite(uchar *s) ;
bit     fread (uchar *s) ;
uint    fquantity(void)  ;

/* TIGERP.C */
void    wttrans(void)   ;

void    ttrans(void)    ;
void    TotalTrans(char);

void    totalizing( )   ;
float   cnetto(void)    ;
void    sendweight()    ;
void    sendidentifier();
void    sendatod()      ;
void    sendfullscale() ;
void    sendtarevale()  ;
void    resersacle()    ;
void    sendzero()      ;
void    sendtare()      ;
//void    sendaddress()   ;
//void    sendprofiID()   ;
void    senderror(uchar);
void    senddecimalpoint() ;
/* * * * * * * * * * * * * * * * * * * * * * * */
/*
  ⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
  ≥           PUBLIC VARIABLES                 ≥€
  ¿ƒ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹€

*/
//   Bit addressable space in internal RAM
ext  bit    ftest;                      // test mode flag
ext  bit    fstab;                      // stability flag
ext  bit    fblink;                     // blink flag (used with tblink)
ext  bit    ftrack;                     // tracking flag
ext  bit    frange;                     // zero tracking range
ext  bit    fsign;                      // negative weight flag
ext  bit    fover;                      // out of limit flag (under/overload)
ext  bit    fdelay;                     // delay to relcon flag (used with tdelay)
ext  bit    fcount;                     // counting mode flag
ext  bit    frestart;
ext  bit    frestart2;
ext  bit    frestart3;
ext  bit    fstatus;
exb  xdata uchar  fautom;               // auto/manual mode flag
ext  bit    fsp;                             // setpoint int
ext  bit    fendset;

//   there is no possible to add also bit flags
//   to RAM bit addressable space
ext  bdata  uchar   options[8];         // user programmable options
ext  bdata  uchar   hidop[1];           // hidden options
ext  data   uint    adtime;
ext  data   bit     fl2x16;
//   Additional bit flags space in external RAM (32 flags)
ext  xdata  uchar   flags[4] ;
#define     fzenb       0               // Z-key usage enable flag
#define     fzuse       1               // Z-key usage in getvalue functions
#define     fpuse       2               // P-key usage in getvalue functions
#define     ftuse       3               // T-key usage in getvalue functions
#define     ffuse       4               // F-key usage (return)
#define     fdoff       5               // Decim offset enable(1)/disable(0)
#define     ftitle      6               // title of listings/labels
#define     fnewwg      7               // new weight flag
#define     fpause0     8               // setpoint delay in the weight = 0 point
#define     fpause8     9               // setpoint delay in self-latching  point
#define     xtare      10               // setpoint's permit after T press only
#define     fpour      11               // pouring(1)/filling(0) for setpoint
#define     fempty     12               // EMPTY case for pouring setpoint
#define     fledoff    13               // LEDs switch ????

ext  xdata  uchar   phase, pass, filcof ;
ext  xdata  uint    spret ;             // setpoint status currently
ext  xdata  uchar   spind ,fwc;             // setpoint position number
ext  xdata  uint    imrel,imrel1 ;             // image of relcon status
ext  xdata  uint    oldimrel,sw1,relnum;
ext  xdata  uchar   flife ;             // relay's longlife time flag
ext  xdata  uchar   nlife,nlife1;             // relay's number for flife = 1
ext  xdata  uchar   fstart, bytest ;    // start flag for new relbox
ext  xdata  uchar   keystatus ;         // relbox keys status
ext  xdata  uchar   speckey ;           // 0x20 - special relbox input

ext  xdata  char    ramp, fdir, dif;
ext  xdata  uint    atod, twodiv, onediv, halfdiv, quartdiv, t4flag,t5flag,ts4flag;
ext  xdata  ulong   weight0, weight1, wtrack,oldtare;
ext  xdata  ulong   zref, znext ;
ext  xdata  ulong   wforce ;            // force zero weight in atod unit
ext  xdata  long    mforce ;            // force zero maximum
ext  xdata  long    weight3 ;           // nonfactorized weight
ext  xdata  long    wdisp, weight2,myw ;    // display weight, previous weight
ext  xdata  long    wprint ;            // last printed weight
ext  xdata  uint    cw1div, cw2div ;    // 1&2division in 'wdisp' units (for cw)
ext  xdata  long    twentydiv ;         // 20 division in 'wdisp' units
ext  xdata  long    fourperstop ;       // 4% of wstop in 'wdisp' units
ext  xdata  long    fiveperstop ;       // 5% of wstop in 'wdisp' units
ext  xdata  long    astop ;             // wstop in 'atod' units
ext  xdata  float   onefifthwstop ;     // 1/5 of wstop in 'atod' units
ext  xdata  float   twofifthwstop ;     // 2/5 of wstop in 'atod' units
ext  xdata  float   onetenthwstop ;     // 0.1 of wstop in 'atod' units
ext  xdata  ulong   onefifthround ;     // 1/5 of round in 'round' units
ext  xdata  ulong   twofifthround ;     // 2/5 of round in 'round' units
ext  xdata  float   dnetto ;            // netto weight for totalizing
ext  xdata  float   rfactor;
ext  xdata  float   wfactor;            // save 'weight' rfactor (by counting)
ext  xdata  uint    wround ,loops,qtrel8of,ppres,swkeyb,qtbacklight;//,zpres,tpres;                // save 'weight' round   (by counting)
ext  xdata  uchar   wdecim ;            // save 'weight' decim   (by counting)
ext  xdata  ulong   wwstop ;            // save 'weight' wstop   (by counting)
ext  xdata  uchar   comport;            // serial port 0 - UART, 1 - xUART
ext  xdata  uchar   qtoff;            // auto turn off counter (use with toff timer)
ext  xdata  uchar   lboff  ;            // lo-bat turn off counter (use with tlobat timer)
ext  xdata  uchar   returncode ;        // return code after edit's routines
ext  xdata  uint    sw_count;
ext  xdata  uchar   ascbuf[80] ;        // primary string buffer
ext  xdata  uchar   secbuf[80] ;        // secondary string buffer
#define decperiod   secbuf[79]          // decimal period position
#define transtare   secbuf[79]          // tare request from computer
#define transzero   secbuf[78]          // zero request from computer

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
ext  xdata  struct  mdat  {
                    uchar md_ferase ;   // erase flag (? - erased data)
                    uchar md_code[13] ; // product code
                    float md_unit   ;   // unit weight
                    ulong md_tare   ;   // it's tare
                    uint  md_order  ;   // weighing quantity
                    float md_total  ;   // weight total
                    uint  md_gorder ;   // weight grandquantity
                    float md_gtotal ;   // weight grandtotal
                    ulong md_ranlo  ;   // low range for auto totalizing
                    ulong md_ranhi  ;   // high range for auto totalizing
                    uchar md_spmax  ;   // setpoint's actual quantity
                    uchar md_wc     ;
                    long  md_zerodiv;
                    long  md_zeroneg;
                    ulong md_sp[3]  ;   // setpoints or target with tollerance
                    }  ;
ext  xdata  struct  mdat  *mptr ;
#define dp  52//44                          // unchanged part of mdat declaration

/*              BIT VARIABLES
     op(bitname, bitnumber) = options bit (bitnumber ranges from 1 to 64)
     hop(bitname, bitnumber) = hidden option (bitnumber ranges from 1 to 15)
*/

op(okilo,   1);         // Kg indicator ON
op(opound,  2);         // Lb indicator ON
op(ototalrel8,3);       // totalizing befor rel 8
op(oneg,    4);         // No displayed negative weight
op(otoneg,  5);         // Auto-zero of negative weight (oneg must be set)
op(ofulz ,  6);         // Full scale zero range(1) or limited range(0)
op(ozmem ,  7);         // Get zero from memory(1)  or autozero(0) at power on
op(ofult ,  8);         // Full scale tare range(1) or 5% of 'wstop'(0)
op(ocumt ,  9);         // Cumulative(1) or toggle(0) tare
op(omant , 10);         // Enable(1) or disable(0) manual tare
op(odisp , 11);         // Dinamic(1)/stable(0) display
op(oduble, 12);         // Double(1)/normal(0) round range
op(othree, 13);         // Triple round range
op(oldprofi,14);        // old profi (0) / new profi (1) bit reset send
op(odfrofi ,15);        // SEND WEIGHT + STATUS TO PROFI (0) / SEND WEIGHT ONLY TO PROFI (1)
op(oconti, 16);         // Weight continuous transmission to computer
op(otrans, 17);         // Transmission to computer(1)/printing on printer(0)
op(oautom, 18);         // Automatic totalizing
op(ogreen, 19);         // green enabel(1) green disabel(0)
op(obigdisp, 20);       // send weight to big display 
//op(obigcom1, 21);       // send weight to big display in com1(1) / com2(0) 
op(onet ,  22);         // net   protocol
op(oprofi, 23);         // profi protocol
op(orel7on,24);         // rel 7 on if not start and weight is 0
op(oricun,  25);        // make tare in ricun

op(otarein , 26);       
op(otareout, 27);       

op(ophalf, 28);         // totalizing new weight after decreasing to half of previous
//op(otear , 29);         // Advance the paper to tear off position
op(obusy , 30);         // 
op(opolar, 31);         // 
op(ocheck, 32);         // checkweigher(1)/setpoints(0) option
op(bcd   , 33);         // BCD communication: (on) - yes
op(bcdout, 34);         // BCD output format: on - bin, off - bcd
op(bcd420, 35);         // BCD 4-20mA format: on - 420, off - op34
op(olight, 36);         // single(1)/double(0) light checkweigher
op(osprev, 37);         // reverse(1)/normal(0) setpoint
op(ospord, 38);         // the next setpoint turn off the previous
op(osptar, 39);         // no setpoint if tare not set (1)
op(osp8  , 40);         // self latching for setpoint
op(bcdsel, 41);         // BCD bisign(on)/unsign(off) selection
op(bcdini, 42);         // BCD status for ini, neg & err cases: 0(on)/-1(off)
op(bcdpol, 43);         // BCD polarity (on: 1-high, 0-low / off: 1-low, 0-high)
op(ospdef, 44);         // setpoint definition partly(1)/totaly(0)
op(odeny , 45);         // disable bit for checkweigher/setpoint

op(ocount, 46);         // Counting mode enable
op(oautof, 47);         // Auto turn off after 4 min without operation
op(otarof, 48);         // disable tare
op(leds  , 49);         // leds without relsys

op(obidsip ,50);         // op37 big display enable
op(onewdisp,51);         // op38 new big diplay (0) / old big dislpay (1)
 
op(oSendWeight, 50);    // send weight if stable(1)

//op(ototnz, 51);         // Subtotal not zeroed after print

op(onist , 52);         // NIST standart
op(odynam, 53);         // Dynamic weight transmit(1)/stable weight inside 2 sec
op(opsign, 54);         // Weight transmit with space(1)/'+'(0) ahead
op(ospace, 55);         // Weight transmit with space(1)/'0'(0) lead characters
op(omehal, 56);         // Container filling/pouring
op(oastrk, 57);         // Weight transmit with * lead character
op(specrel, 58);        // rel 6 go with input 6 and rel 8
op(oforcetar, 59);      // rel 6 go with input 6 and rel 8
op(onostab,60);         // no stab in auto totelaizing
//op(onotitle,61);        // no titles in print
op(opuls,62);           // puls in last setpoint
op(ozerotrace,63);      // no zero tracing wen =1
op(oerr10,64);          // chack atod end return error 10
hop(hopdrift, 1);       // Drift enable hidden option
hop(hopref, 2);         // Display refresh enable hidden option

/* * *  NOVRAM DATA AREA  * * */

ext xdata   uchar    novdata[56];
                     // PAGE 0
#define     novsig   word(novdata, 0)       // NOVRAM signature
#define     factor   real(novdata, 2)       // Real factor
#define     trimval  novdata[6]             // Digital trimmer setting
#define     decim    novdata[7]             // Digits after decimal period
                     // PAGE 1
#define     wfull    quad(novdata, 8)       // Full scale definition
#define     wstop    quad(novdata, 12)      // Full scale definition
                     // PAGE 2
#define     round    word(novdata, 16)      // Rounding unit for weight
#define     com1     novdata[18]            // Internal UART control byte
#define     com2     novdata[19]            // External UART control byte
#define     profiadd novdata[20]            // profibus address   
#define     profiID1 novdata[21]            // profibus ID
#define     hiatod   word(novdata, 22)      // Hidden atod for trimmer set
                     // PAGE 3
#define     samprate novdata[24]            // Sampling rate
#define     filmax   novdata[25]            // filter stable coefficient
#define     filrate  novdata[26]            // rate of increase fo filcof
#define     stabin   novdata[27]            // stability request (fast timer)
#define     stabout  novdata[28]            // stability request refresh
#define     stabof   novdata[29]            // out of stability condition
#define     hidnov   novdata+30             // hidden options storage
                     // PAGE 4
#define     nforce   quad(novdata, 32)      // force zero weight restriction
#define     dforce   novdata[36]            // force zero timer
#define     bcdisp   novdata[37]            // bcd display part
#define     reserve                         //
#define     opset    novdata[39]            // desired opset number
                     // PAGE 5
#define     zref0    quad(novdata, 40)      // zero reference by calibration
#define     sn       quad(novdata, 44)      // serial number
#define     net      novdata[48]            // scale net number

#define     profiID  word(novdata, 49)      // serial number
#define     setplim  novdata[51]            // set point number
                     // PAGE 15             User programmable options
/* * * * * * * * * * * * * * * * * * * * * * * */
/*
  ⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
  ≥        BATTERY PROTECTED VARIABLES         ≥€
  ¿ƒ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹€

        Use the keyword "exb" before all the declarations
*/
#define  spnum  16                       // setpoints total number
#define  splim1  3
#define  splim   4
exb  xdata  ulong  is_battery ;             // is battery present (password = 1955)
exb  xdata  ulong  lastzref ;               // last zero reference
exb  xdata  uint   dtime,dpuls,dpulsl;      // dead time delay between setpoints
exb  xdata  long   dzone ;                  // dead weight zone for setpoints
exb  xdata  uchar  acode[13] ;              // actual code number
exb  xdata  ulong  tare ,rel8off ;          // tare properly
exb  xdata  ulong  OneTare  ;               //
exb  xdata  uint   tuint,qtemp  ;                 // tare uint
exb  xdata  float  kunit,kunit1 ;                  // unit weight
exb  xdata  uchar  spmax ;                  // max quantity of setpoints (spmax <= spnum)
exb  xdata  uint   order, gorder ;          // order & grandorder number
exb  xdata  float  total, gtotal,qtbl ;          // total & grandtotal for acode
exb  xdata  uint   dtime0, dtimez ;          // delay in zero point after self-latching (sp)
exb  xdata  uint   dtime8 ;                 // delay in self-latching point (sp)
exb  xdata  long   zerodiv ;                // positive zero range for relayZ(ero)
exb  xdata  long   zeroneg ;                // negative zero range for start-tare of relcon
exb  xdata  uint   greendiv,sptime1[3];               // green range for checkweigher
exb  xdata  ulong  spdata[spnum] ;          // setpoint's values
exb  xdata  uchar  sptime[4]  ;  // relay's longlife times for checkweigher
exb  xdata  uchar  rel3on[4] ;              // 
exb  xdata  ulong  target, tollo, tolhi ;   // checkweigher's target and tollerance values
exb  xdata  ulong  ranlo, ranhi ;// low and high ranges for auto totalizing
exb  xdata  uchar  fsend1, ftranzero, ftranstare;

exb  xdata  uint   drel3on, oldimrel2;
exb  xdata  uchar  flock, snum;
ext  xdata  float  fdisp ;

#ifndef exd
#define exd extern
#endif
#define     darea  938                      // 14 'mdat' structures for 2 Kbytes BRAM
ext  xdata  uint   marea ;                  // mcode memory area real dimension
exb  xdata  uchar  *meof ;                  // pointer to mcode 'eof'
exd  xdata  uchar  mcode[darea] ;           // code area
/* * * * * * * * * * * * * * * * * * * * * * * */

#define netsetpoint 'P'
#define netcweigher 'C'
#define getrelay    'S'
#define setrelay    'R'
#define netweight   'W'
#define nettare     'T'
#define netzero     'Z'

#define exitfun     fullsec;

#define request     netstring[0]
#define savuest     netstring[4]

ext  xdata  uchar   await, fcounttoggle, fcountstart ;
ext  xdata  uchar   netcs ;         // net request checksum
ext  xdata  uchar   netstring[5] ;  // net scale number
ext  xdata  uchar   netbuf[160] ;   // net buffer (for sp/cw command string receiption
ext  xdata  uchar   counter ;       // command string bytes counter
ext  xdata  uchar   phext ;         // network's phase next step
ext  xdata  ulong   xnet;
ext  xdata  uchar   ffkey, fexitcode;

bit     setrans(uchar) ;
bit     network(uchar) ;
void    answer(uchar *) ;
uchar   cs(uchar *) ;
uchar   cs1(uchar *);