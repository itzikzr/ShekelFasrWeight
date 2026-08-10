#include "fanbase.h"

void    test0(void);
void    test1(void);
void    test2(void);
void    test23(void);
void    test3(void);
void    show_fac(void);
void    test4(void);

void    testmode()
{
/*
    if (!ftest){
        ftest = round0 = 1;
        adcom(0, adful);
    }
*/
    ferror = 0;
    if (comstat()) serial();
    if (atod(0))
    {
        if (fun==NULL) fun = test0;
        fun();
    }
    if (kbhit()) testkey(getch());
}

void    test0()
{
    adcom(0, adwey);
    brut = 0;
    adcom(0, adlim);                    // remove zero limits
    adcom(0, adwey);
    dmr = dml = 1;
    dur = dul = dlr = dll = dpcs = 0;
    fun = test1;
}

void    test1()
{
    xdata ulong n, z, b;

    switch (errnum)
    {
        case 2: brut = -99999; break;
        case 3: brut =  99999; break;
    }

    errnum = 0;
    status(0);
//    display(brut, disform);

    z = zref2; z <<= 1;
    if (hibyte(z) & 1) hibyte(z) == 0xff;
    else               hibyte(z) == 0;
    adcom(0, adraw);
    b = brut;
    brut += 0X80; brut >>= 8; 
    if (inbyte(brut, 1)) hibyte(brut)--;
    n = (ulong)(abs(brut - zref) * (fac2float(fac0)/8));
    if (fan2gul(b) < fan2gul(z) && n) fng = 1;
    else                              fng = 0;
    display(n, disform);

    if (testbit(falter))
    {
        fun = test2;
        dul = dll = dml = 0;
        dur = dlr = dmr = 1;
        dkg = dlb = dzero = dpcs = 0;
        adcom(0, adraw);
    }
}

void    test2()
{
    switch (errnum)
    {
        case 2: brut = -99999; brut <<= 8; break;
        case 3: brut =  99999; brut <<= 8; break;
        default:;
    }
    errnum = 0;
    display(fan2gul(brut), 0);
    if (testbit(falter)) fun = test23;
}

void    test23()
{
    dmr = dur = dlr = 0;
    dul = dll = dml = 1;
    dkg = dlb = dzero = dpcs = 0;

    switch (errnum)
    {
        case 2: brut = -99999; brut <<= 8; break;
        case 3: brut =  99999; brut <<= 8; break;
        default:;
    }
    errnum = 0;
    brut += 0X80;
    brut >>= 8;
    if (inbyte(brut, 1)) hibyte(brut)--;
    if (fan2gul(brut) == 195) puts("\n STOP ");
    else                      display(brut, 0);
    if (testbit(falter)) fun = test3;
}

void    test3()
{
    xdata uint  n;

    dmr = dml = dbat = 1;
    n = getbat(0);
    hibyte(n) = 0;
    printf("\n =%3u=", n);
    if (testbit(falter)) fun = show_fac;

}

void    show_fac()
{
    xdata uint  n;
    xdata float f;

    dbat = 0;
    n = (uint)DecimalPointNum(disform);
    switch(n){
        case 0: f = fac2float(fac0) * 1000; break;
        case 1: f = fac2float(fac0) * 100 ; break;
        case 2: f = fac2float(fac0) * 10  ; break;
        case 3: f = fac2float(fac0)       ; break;
        case 4: f = fac2float(fac0) / 10  ; break;
        case 5: f = fac2float(fac0) / 100 ; break;
    }
    if (full1) f = (f / ((float)full1 / (float)(fpow(10,n)*10)));
    printf("\n%7.5f",f);
    if (testbit(falter)) fun = test4;
}

void    test4()
{
    dml = dll = dur = dmr = dpcs = 0;
    getver(0);
    printf("\n=%5lu", brut);
    if (testbit(falter)) fun = NULL;
}

float   fac2float(ulong n)
{
    float   x;
    uchar   m;

    m = 137 - hibyte(n);
    do {
        n <<= 1;
        m--;
        if (hibyte(n)&1) break;
    } while (m);
    hibyte(n) = m;
    n >>= 1;
    inbyte(x, 0) = inbyte(n, 3);        // IEEE float conversion with
    inbyte(x, 1) = inbyte(n, 2);        // Keil-Franklin inconsistency
    inbyte(x, 2) = inbyte(n, 1);        // Long is big endian
    inbyte(x, 3) = inbyte(n, 0);        // while Float is little endian
    return (x);
}

ulong   float2fac(float f)
{
    inbyte(temp, 0) = inbyte(f, 3);        // IEEE float conversion with
    inbyte(temp, 1) = inbyte(f, 2);        // Keil-Franklin inconsistency
    inbyte(temp, 2) = inbyte(f, 1);        // Long is big endian
    inbyte(temp, 3) = inbyte(f, 0);        // while Float is little endian
    temp <<= 1;
    hibyte(temp) = 136 - hibyte(temp);
    hibyte(temp) <<= 1;
    hibyte(temp)++;
    temp >>= 1;
    return (temp);
}


ulong   getver(uchar device)
{
    uchar   n;

    n = adtype[device];
    if (adcom(device, adver))
    {
        adsamp = 0;
        while (!atod(device)) watchdog();
    }
    else brut = 0;
    adcom(device, n);
    adsamp = errnum = ferror = 0;
    return (brut);
}

void    testkey(uchar k)
{
    switch (k){
        case 'Z': getzer(0, 0); break;
        case 'T': falter = 1;   break;
        default: reboot();
    }
}
