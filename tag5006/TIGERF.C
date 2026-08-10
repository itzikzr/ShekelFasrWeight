#include "tiger.h"
/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³                 Miscellaneous  functions                 ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

void    wakeup(void)
{
    uchar n;
    relbuf[0] = -1;
    relbuf[1] = 1;
    for (n=2;n<10;n++) relbuf[n] = 0X11;
    TH0 = 1;
    TR0 = 1;
    ET0 = 1;
    EA = 1;
    tonoff = 20;
    while (tonoff)
    {
        if (!INT1) tonoff = 20;
        watchdog();
    }
    EX1 = 1;
}

void light(uchar color) small
{
    disled = color;
}

void    backlight(uchar bl) small
{
    if (bl) discom |= 0X10;
    else discom &= 0XEF;
}

void    drefresh(uchar time) small
{
    if (!trefresh)
    {
        flcdon = 1;
        trefresh = time;
    }
}

uchar   password() small
{
    uchar k;
    k = 0;
    if (kbhit())
    {
        switch (getch())
        {
            case 'Z': pass += pass;
                break;
            case 'T': pass += pass + 1; /*break;*/
                t2 = trisec;
                do
                {
                    watchdog();
                    if (!t2)
                    {
                        timeout = 0;
                        puts("\n TEST ");
                        while(keypress()) watchdog();
                        return 1;
                    }
                }
                while(keypress());
                break;
            default:;
            }
        timeout = twosec;
        while ((passcode[k])&&(passcode[k]!=pass)) k++;
        if (passcode[k++]) timeout = 0;
        else k = 0;
    }
    return (k);
}

void    pattern() small
{
    uchar k;
    if (!tblink)
    {
        tblink = onesec;
        k = unput(1);
        if (isdigit(k))
        {
            if (k<'9') k++;
            else k = '0';
            cursor = 1;
            while (cursor<7) putchar(k);
        }
    }
}

void    setdiv()
{
    onediv = (uint) ((((float) wfull)/factor)/divnum);
    twodiv = onediv<<1;
    halfdiv = onediv>>1;
    quartdiv = onediv>>2;
    if (!byte(onediv, 0)) onediv = 0X100;
    if (!byte(halfdiv, 0)) halfdiv = 0X100;
    if (!byte(quartdiv, 0)) quartdiv = 0X100;
    cw1div = onediv * factor + 0.5 ;      //  1 div in 'wdisp' units for cweigher
    cw2div = twodiv * factor + 0.5 ;      //  2 div in 'wdisp' units for cweigher
    twentydiv = onediv*factor * 20 ;      // 20 div in 'wdisp' uints
}

void    execute(void (*f)())
{
    f();
}

bit     novsave(uchar k)
{
    fblink = 1;
    if ((k-1)>(sizeof(novdata)>>3)) k = sizeof(novdata)>>3;
    memcpy(hidnov, hidop, sizeof(hidop));
    while ((k)&&(fblink))
    {
        k--;
        fblink &= novout(k, novdata+(k<<3));
    }
    fblink &= novout(15, options);
    return (testbit(fblink));
}

void    loadef()
{
    memcpy(novdata, novdef, sizeof(novdata));
    memcpy(options, opdef, sizeof(options));
    memcpy(hidop, hidnov, sizeof(hidop));
    rfactor = factor;
    if (round) rfactor /= round;
    else round = 1;
    setdiv();
    byte(adtime, 1) = samprate;
    byte(adtime, 0) = samprate;
    comini(com1);
    xcomini(com2);
    flag(fkilo, okilo);
    flag(fpound, opound);
}

void    errors(uchar errnum, uchar errlevel)
{
    relset((bcdini)? 0: -1) ;
    clrscr();
    pass = 4;
    printf("\nERR-%02u", (uint) errnum);
    timeout = twosec;
    if (errlevel) ftest = 0;
    if (ftest) getch();
    else while (timeout)
    {
        senderror(errnum);
        if (!errlevel) ftest = password();
        watchdog();
    }
    if (ftest) recycle();
    else execute(NULL);
}

void    stackok()
{
    if (DWORD[0X3F]) errors(1, 1);
}

bit     xbit(uchar n, uchar r)
{
    //      n = bit number inside 'flags' space
    //      r = 0 - off, 1 - on, 2 - status test

    uchar nbyte, d = 1 ;
    bit   ret  ;

    nbyte = n>>3 ;
    d <<= n - (nbyte<<3) ;
    ret   = flags[nbyte]&d ;
    switch(r)
    {
        case 0  :  flags[nbyte] &= ~d ;
            break ;
        case 1  :  flags[nbyte] |=  d ;
            break ;
        case 2  :                       break ;
        }
    return ret ;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
void beep(int n)
{
    discom &= 0xf0;
    discom |= 0xf0+8+n;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
void    dummy()
{
    tools() ;
    putcom(0) ;
    xputcom(0) ;
    voltage();
    allcom();
    chargen(0,0);
    htrans(0);
    htrans_new(0);
}
/* * * * * * * * * * * * * * * * * * * * * * * */