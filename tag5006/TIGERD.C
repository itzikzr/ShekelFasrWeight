#define  ext
#include "tiger.h"
/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³                      External  data                      ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/
#define id1  (prog * 0x1000000)
#define id2  (year  % 10) * 0x100000
#define id3  (month % 10) * 0x10000
#if month > 9
    #define id4  (day/10 + 5) * 0x1000
#else
    #define id4  (day/10) * 0x1000
#endif
#define id5  (day%10) * 0x100

code  ulong  id = id1 + id2 + id3 + id4 + id5 ; // software identifier

code  uchar  keylist[] =        // Keyboards scancode decoding array
    "..789Z......FC0."
    "..456T.....P321."
    "..123P.....T654."
    "..0CF......Z987."
    ".TPzZxtp........"            // z = P&Z, t = P&T, x = T&Z, p = P&T&Z
    "................";

code  uchar novdef[] =
{                      // NOVRAM default values
    // PAGE 0
    0XB1, 0X35,                 // novsig  = 0xb135
    0, 0, 0X80, 0X3B,           // factor  = 0.00390625
    33,       // trimval = 33
    3,    // decim   = 3
    // PAGE 2
    0, 0, 0X3A, 0X98,               // wfull   = 15000
    0, 0, 0X3A, 0X98,     // wstop   = 15000
    // PAGE 2
    0, 5,                                   // round   = 5
    0X65,                             // com1    = 9600,N,8
    0X65,                       // com2    = 9600,,8
    2,                   // profibus address    = 0
    0,                 
    0x08, 0x98,     // hiatod  = 2200
    // PAGE 3
    150,                                    // samprate
    4,                                 // filmax
    30,                             // filrate
    100,                        // stabin
    4,                     // stabout
    6,                  // stabof
    3, 0,            // hidden options
    // PAGE 4
    0, 0, 0, 10,                            // nforce  = 10
    100,                       // dforce  = 10.0
    0, 0,                  //
    1,           // opset   = 1
    // PAGE 5
    0x0, 0x7, 0x8A, 0xBE,                   // zref0 = (near 2200)
    0, 0, 0, 0,
    0,                                      // net id
    0xe3, 0x06,                             // profibus ID
    0x10,                                   // set point number
} ;    //

code  uchar opdef[] =
{                   // NOVRAM user options
    0xb9, 0x86, 0x81, 0x0c, 0x98, 0x00, 0x12, 0,  // setpoint + continuous transmit
    0xb9, 0x06, 0x81, 0x0c, 0x98, 0x00, 0x12, 0,  // setpoint + manual transmit 
    0xb9, 0x06, 0x83, 0x0c, 0x98, 0x00, 0x12, 0,  // setpoint + manual transmit profi
    0xb9, 0x06, 0x81, 0x0c, 0x98, 0x00, 0x12, 0,  // setpoint + manual transmit net 
    0xb9, 0x86, 0xc0, 0x2c, 0x98, 0x00, 0x12, 0,  // setpoint + CITIZEN IDP3545

    0xb9, 0x06, 0xa1, 0x0c, 0x98, 0x00, 0x12, 0,  // setpoint + manual transmit  net 
    0xd9, 0x04, 0xc1, 0x0c, 0x98, 0x80, 0x12, 0x40,  // setpoint + manual transmit profi
    0xb9, 0x06, 0x98, 0x0c, 0x98, 0x00, 0x12, 0,  // setpoint + manual transmit + big display
    0xdd, 0x05, 0x02, 0x0d, 0xd8, 0x80, 0x51, 0x40,  // setpoint + SEIKOSHA  (tanks) + ricun
    0,    0,    0,    0,    0,    0,    0, 0,     // setpoint 10

//  8-1  16-9  24-17 32-25 40-33 48-41 56-49 64-57       options number

    0xb9, 0x86, 0x81, 0x8c, 0x88, 0x20, 0x13, 0,  // checkweigher + continuous trans
    0xb9, 0x06, 0x81, 0x8c, 0x88, 0x20, 0x13, 0,  // checkweigher + manual transmit
    0xb9, 0x06, 0x82, 0x8c, 0x88, 0x20, 0x11, 0,  // checkweigher + SEIKOSHA
    0xb9, 0x46, 0x80, 0xac, 0x88, 0x20, 0x11, 0,  // checkweigher + CITIZEN
    0xb9, 0x06, 0xc0, 0xac, 0x88, 0x20, 0x13, 0,  // checkweigher + CITIZEN IDP3545

    0xb9, 0x06, 0xa1, 0x8c, 0x88, 0x20, 0x13, 0,  // checkweigher + manual transmit + net
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 7
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 8
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 9
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 10

//  8-1  16-9 24-17 32-25 40-33 48-41 56-49 64-57       options number

    0xb9, 0x86, 0x81, 0x0c, 0x99, 0x00, 0x13, 0,  // bcd
    0xb9, 0x86, 0x81, 0x0c, 0x9d, 0x06, 0x13, 0,   // 4-20
    0xd9, 0x86, 0x81, 0x0c, 0x99, 0x80, 0x13, 0x40,  // bcd tanks
    0xd9, 0x86, 0x81, 0x0c, 0x9d, 0x86, 0x13, 0x40,   // 4-20 tanks
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 7
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 8
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 9
    0,    0,    0,    0,    0,    0,    0, 0,
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 5
    0,    0,    0,    0,    0,    0,    0, 0,  // checkweigher 6

//  8-1  16-9 24-17 32-25 40-33 48-41 56-49 64-57       options number

    0xb9, 0x86, 0x81, 0x0c, 0x98, 0x00, 0x13, 0,  // setpoint + continuous transmit
    0xb9, 0x06, 0x81, 0x0c, 0x98, 0x00, 0x13, 0,  // setpoint + manual transmit
    0xb9, 0x06, 0x82, 0x0c, 0x98, 0x00, 0x11, 0,  // setpoint + SEIKOSHA
    0xb9, 0x46, 0xc0, 0x2c, 0x98, 0x00, 0x11, 0,  // setpoint + CITIZEN
    0xb9, 0x86, 0xc0, 0x2c, 0x98, 0x00, 0x13, 0,  // setpoint + CITIZEN IDP3545

    0xb9, 0x06, 0xa9, 0x0c, 0x98, 0x00, 0x13, 0,  // setpoint + manual transmit + net
    0,    0,    0,    0,    0,    0,    0, 0,
    0,    0,    0,    0,    0,    0,    0, 0,
    0,    0,    0,    0,    0,    0,    0, 0,
    0,    0,    0,    0,    0,    0,    0, 0,

//  8-1  16-9 24-17 32-25 40-33 48-41 56-49 64-57       options number

    0xd9, 0x86, 0x81, 0x0c, 0x98, 0x80, 0x53, 0x40,  // setpoint + continuous transmit (tanks)
    0xd9, 0x06, 0x81, 0x0c, 0x98, 0x80, 0x53, 0x40,  // setpoint + manual transmit  (tanks)
    0xd9, 0x06, 0x82, 0x0c, 0x98, 0x80, 0x51, 0x40,  // setpoint + SEIKOSHA  (tanks)
    0xd9, 0x46, 0xc0, 0x2c, 0x98, 0x80, 0x51, 0x40,  // setpoint + CITIZEN   (tanks)
    0xd9, 0x86, 0xc0, 0x2c, 0x98, 0x80, 0x53, 0x40,  // setpoint + CITIZEN IDP3545 (tanks)

    0xd9, 0x06, 0xa1, 0x0c, 0x98, 0x80, 0x53, 0x40,  // setpoint + manual transmit  (tanks) + net
    0,    0,    0,    0,    0,    0,    0, 0,
    0,    0,    0,    0,    0,    0,    0, 0,
    0,    0,    0,    0,    0,    0,    0, 0,
    0,    0,    0,    0,    0,    0,    0, 0,

    0,    0,    0,    0,    0,    0,    0, 0  // sop 51
    /*
    8-1  16-9 24-17 32-25 40-33 48-41 56-49 64-57       options number
    */
};

code  uchar passcode[] =
{                // Password array
    0X93,                               // T-Z-Z-T-Z-Z-T-T
    0XB9,                               // T-Z-T-T-T-Z-Z-T
    0
};                                // Array must end with 0

code  uchar ubaud[] =           // Internal UART baud rate decoder
{ -192, -96, -48, -24, -12, -6, -4, -3 };

code  uchar xbaud[] =           // External UART baud rate decoder
{ 4, 5, 6, 9, 0XB, 0XD, 0XE, 0XF };

code  uchar *ubaudisp[] =       // Internal UART baud rate display string
{ "  3", "  6", " 12", " 24", " 48", " 96", "144", "192" };

code  uchar *xbaudisp[] =       // External UART baud rate display string
{ "  3", "  6", " 12", " 24", " 48", " 96", "192", "384" };

code  uchar *uformat[] =        // UART communication format
{ "N7", "E7", "N7", "O7", "N8", "E8", "N8", "O8" };

code  float  mload[] =      // load value rounding
{ 0.1, 0.2, 0.3, 0.5 } ;

code  uchar *namlist[] =
{  // Product name table
    " product - 1 name ",
    " product - 2 name ",
    " product -20 name "
} ;

code  ulong  null[] = { 0x00, 0x00 } ;
code  uchar  crlf[] = { 0x0d, 0x0a } ;
code  uchar  ack[ ] = { 0x06, 0x00 } ;
code  uchar  nak[ ] = { 0x15, 0x00 } ;
