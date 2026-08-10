#include "fanbase.h"

void    comset(uchar);    // string pointer type for the RS232 interrupt handler

void    comini(uchar com)
/*
    com bit assignment = ---  BIT  ODD  PEN  BR3  BR2  BR1  BR0
        BIT     8 bits format if set, 7 bits format if clear.
        ODD     Odd parity if set, even parity if clear
        PEN     Parity bit appended if set. No parity if clear.
        BRn     Baud rate code, ranging from 0 to 9:
    code:     0    1     2     3     4     5     6      7     8      9
    baud:   300  600  1200  2400  4800  9600  14400  19200  28800  38400
    Baud codes above 9 are reduced to 9 (38400)
*/
{
    ES = 0;                         // stopping the serial interrupt
    fpen = com & 0X10;
    fodd = com & 0X20;
    fbit = com & 0X40;
    com &= 0XF;
    RCAP2H = hibyte(ubaud[com]);
    RCAP2L = lobyte(ubaud[com]);
    T2CON = 0X34;                   // using timer 2
    if (fbit&fpen) SCON = 0XDE;     // UART control
    else SCON = 0X5E;
    flush();                        // flush buffer before starting
    ES = 1;                         // enabling the serial interrupt
}

uchar   getcom()
{
    while (!opstop()) watchdog();
    if (fkey) rsbyte = 0;
    else rsbyte = * (uchar pdata *) rsmpo;
    if ((++rsmpo)==(uchar) (bufin+insize)) rsmpo = (uchar) bufin;
    if (!fbit) rsbyte &= 0X7F;          // dropping the parity bit
    return (rsbyte);
}

void    comstring(uchar *s)
{
    while (fsend) watchdog();
    rsoco = strlen(s);
    lobyte(rsopo) = inbyte(s, 1);
    hibyte(rsopo) = inbyte(s, 2);
    comset(inbyte(s, 0));
    fsend = 1;
    TI = 1;
}

void    binstring(uchar *s, uchar n)
{
    while (fsend) watchdog();
    rsoco = n;
    lobyte(rsopo) = inbyte(s, 1);
    hibyte(rsopo) = inbyte(s, 2);
    comset(inbyte(s, 0));
    fsend = 1;
    TI = 1;
}

uchar pontype(uchar c)   // pointer type decoder, compiler version dependent
{         // 0 = DATA, 1 = XDATA, 2 = PDATA, 3 = CODE
    #if __C51__ < 400     // unknwon versions
        #error Unknown compiler version
        #elif __C51__ < 500
        if ((--c)>2) c = ~c;   // version 4
    #endif        // version 5 and above
    c &= 3;
    return (c);
}

void comset(uchar n)
{
    n = pontype(n);
    rsmode &= 0XDD;
    F0 = n & 1;
    ACC = n;
    ACC = (PSW + 1) & 0X22;   // moving the parity bit to F1
    rsmode |= ACC;
}