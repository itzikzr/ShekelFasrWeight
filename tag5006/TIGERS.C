#include "tiger.h"
/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³            Startup : peripheral initialization           ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

bit  main(void)
{
    char s[40];
    bimage = bdef;
    portb  = bimage;
    puts("\n888888");
    icini();
    wakeup();
    lcdini(0);
    swkeyb=0;
    bell(2);
    for (atod=0;(!fstab)&&(atod<((uint) sizeof(novdata)));atod+=8)
        fstab |= ~novin(novdata+atod, atod>>3);
    fstab |= ~novin(options, 15);         // User programmable options
    memcpy(hidop, hidnov, sizeof(hidop)); // hidden options
    relset((bcdini)? 0: -1) ;    // bcd initial status
    if (!ocheck) disled = -1;
    if (!round) round = 1;
    sw1=0;
    if (testbit(fstab))
    {
        loadef();
        ftest = 0;
        errors(3, 0);
    }
    comini(com1);
    xcomini(com2);
    if (!trimset(trimval))
    {
        ftest = 0;
        errors(2, 0);
    }
    rfactor = factor;
    if (round) rfactor /= round;
    else round = 1;
    setdiv();
    byte(adtime, 1) = samprate;
    byte(adtime, 0) = samprate;
    clrscr();
    flag(fkilo, okilo);
    flag(fpound, opound);
    if (memcmp(novdef, novdata, 2))
    {
        loadef();
        recycle();
    }
    lcdstart();
    if (testbit(ftest))
    {
        puts("\n------");
        timeout = twosec;
    }
    else
    {
        puts("\n999999");
        timeout = 100;
    }
    while (kbhit()) getch();              // clearing the keyboard buffers
    while (timeout)
    {
        pattern();
        ftest = password();
        while (!adata);
        atod = atodin();
        if (fstab) filter(&zref, atod);
        else
        {
            if (atod)
            {
                fstab = (zref==atod);
                if (fstab) zref <<= 8;
                else zref = atod;
            }
        }
    }

    if (atod < a2dlim) errors(4, 0) ;
    if (atod > (uint)(0xffff - a2dlim)) errors(5, 0) ;
    weight0 = znext = zref;
    fstab = 0;
    fcountstart = 1;
    tgostab = stabin;
    if (pass-pass) dummy();
    if (!ftest)
    {
        defpour() ;               // defpour -  pouring/filling for setpoint
        initxbits() ;
        battery() ;
        initvalue() ;
        memtest() ;
    }
    snum = ftranzero = ftranstare = 0;
    if ((fautom != 0) && (fautom != 1)) fautom = 0;
    if (ototalrel8) fautom = 0;
    fstatus = 1;
    return(ftest) ;
}
void    initxbits(void)
{
    xbit(ftitle , on) ;
    xbit(fnewwg , on) ;
    xbit(fpause0, on) ;
}
void    initvalue(void)
{
    weight2 = -(cw2div + 1) ;
    fourperstop = (float)zerodiv * 0.04 ;   // 4% of wstop in 'wdisp' units
    fiveperstop = (float)wstop * 0.05 ;   // 5% of wstop in 'wdisp' uints
    astop = wstop/factor ;                // wstop translated to atod units
    onefifthwstop = wstop/factor/5 ;      // 1/5 of wstop in 'atod' units
    twofifthwstop = onefifthwstop*2 ;     // 2/5 of wstop in 'atod' units
    onetenthwstop = onefifthwstop/2 ;     // 0.1 of wstop in 'atod' units
    onefifthround = round/5 ;             // 1/5 of round in 'round' units
    if (!onefifthround) onefifthround = 1 ;
    twofifthround = 2*round/5 ;           // 2/5 of round in 'round' units
    if (!twofifthround) twofifthround = 1 ;
    if (dforce) wforce = nforce/factor;   // force zero in 'atod' units
        if (ocount)
    {                         // saved rfactor, round and decim
        wfactor = rfactor ;               //      by counting mode
        wround  = round ;
        wdecim  = decim ;
        wwstop  = wstop ;
        if (kunit) toggle0() ;
    }
    lboff = twomin ;

}
#define _1Kbytes  1024
#define _2Kbytes  2048
void    memtest()
{
    uint    m ;

    m = ((uint)acode + memsize)/(_1Kbytes) ;
    marea = darea + (m/2 - 1)*(_2Kbytes) ;
}
