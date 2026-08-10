#include "fanbase.h"

bit     icini()
{
    uchar   n;
    uint    t;
    t = biostime(50);
    n = 12;
    do
    {
        SCL = 0;
        watchdog();
        icstop();
        if (!(SCL&SDA)) n = 12;
        if (btm(t)) n = 0;
    }
    while (n--);
//    delay(16, 0);
    return (SCL&SDA);
}

void    icset(uchar mode)
{
    fstop = fsync;
    do watchdog();
    while (fsync);
    icini();
    if (mode)
    {
        icmode &= 0X38;
        if (mode&dispen) flcd = 1;
        if (mode&adrun) frun = 1;
        else if (mode&adfix)
        {
            if ((mode&7)>((icmode>>3)&7)) mode = icmode>>3;
            icmode |= (mode&7);
        }
        fsync = 1;
    }
}

bit     adcom(uchar device, uchar op)
{
    uchar   m, n, k;
    xdata   ulong   arg;
    if (op>iccom) op = 0;
    if (device>7) op = 0;
    k = adtype[device];
    tatod = hafsec;
    arg = brut;
    while ((tatod)&&(op)) {
        adtype[device] = op;
        icset(0);
        n = 0;
        while ((n++)<8) {
            if (adout(device+device+adbase)) {
                if (adout(op)) {
                    if (op>5) {
                        for (m=0;m<4;m++) {
                            if (!adout(inbyte(arg, m))) m = 10;
                        }
                        if (m<10) n = 10;
                    }
                    else n = 10;
                }
            }
            if (n<10) {
                icstop();
                delay(8, 0);
            }           
        }
        if (n<10) op = 0;
        icstop();
        delay(8, 0);
        fsync = 1;
        if (op) {
            while ((!atod(device))&&(tatod)) watchdog();
            ferror = 0;
            errnum = 0;
            while ((!atod(device))&&(tatod)) watchdog();
            if ((errnum)||(!tatod)) brut = arg+1;
            if (op==zcal) op = 1;   // calibration commands are not echoed
            if (op==wcal) op = 1;
            if (op==adset) op = 1;  // reset command is not echoed
            if (op==tcal) {
                arg = brut;
                adtype[device] = k;
            }
            if (op==adtar) arg = brut;      // tare functions
            if (op==adcnt) {                // counting functions
                switch (hibyte(arg)) {
                    case 0X81:
                    case 0X84:
                    case 0X85: arg = brut;
                        break;
                    default:;
                }
            }
        }
        if (op<6) brut = arg;
        if (brut==arg) break;
        else delay(8, 0);
    }
    if (brut!=arg) op = 0;
    if (!op) adtype[device] = k;
    tref = 3;
    fresh = 0;
    return (op);
}

bit     adload(uchar device)
{
    if (device<8)
    {
        if (!adcom(device, adwey)) device = -1;     // device in weighing mode
    }
    if (device<8)
    {
        if (!device) brut = fac0;
        else
        {
        }
        if (!adcom(device, adfac)) device = -1;     // load factor
    }
    if (device<8)
    {
        if (!device) brut = zref0;
        else
        {
        }
        if (!adcom(device, adref)) device = -1;     // load zero reference
    }
    if (device<8)
    {
        if (!device) brut = full0;
        else
        {
        }
        if (!adcom(device, adful)) device = -1;     // load round/full scale
    }
    if (device<8)
    {
        if (!device) brut = param0;
        else
        {
        }
        if (!adcom(device, adpar)) device = -1;     // load parameters
    }
    if (device<8)
    {
//        if (!device) brut = track0 & 0xff0000ff; //disabel zero tracking
        if (!device) brut = track0;
        else
        {
        }
        if (!adcom(device, adran)) device = -1;     // load zero tracking range
    }
    if (device<8)
    {
        if (!device) brut = hitco0;
        else
        {
        }
        if (!adcom(device, hitco)) device = -1;     // High TCO coefficients
    }
    if (device<8)
    {
        if (!device) brut = lotco0;
        else
        {
        }
        if (!adcom(device, lotco)) device = -1;     // Low TCO coefficients
    }
    if (device<8)
    {
        if (!device) brut = zerlim0;
        else
        {
        }
        if (!adcom(device, adlim)) device = -1;     // load ZERO limits
    }
    if (!adcom(device, adwey)) {
        if (!adcom(device, zcal))  device = -1;         // device in weighing mode
        if (!adcom(device, adwey)) device = -1;         // device in weighing mode
    }
    return (device<8);
}

uchar   adini()
{
    uchar   n;
    icset(0);
    icmode = 0XC8;          // detection of A/D devices
    fsync = 1;
    delay(250, 0);
    n = adsamp;
    icset(0);
    icmode = -8;
    adnum = 0;
    while (n)
    {
        icmode += 8;
        adnum++;
        n >>= 1;
    }
    if (adnum)
    {
////        adcom(0, zcal);     // fix error 3 bug in startup
        n = adload(0);                  // single device operation
        if (n) icset(dispen+adfix);
        else adnum = 0;
    }
    return (adnum);
}

bit     atod(uchar n)
{
    uchar   m, p;
    p = setbit(n);
    n &= 7;
    if (p&adsamp)
    {
        while (p&adsamp)
        {
            adsamp ^= p;
            brut = adbuf[n];
        }
        raw = brut;
        m = hibyte(brut);
        adstat[n] = m;
        if (m==0XFF)
        {
            for (m=0;m<4;m++)
            {
                watchdog();             // resampling against phase error
                while (!(adsamp&p));
                adsamp ^= p;
                if (brut!=adbuf[n]) brut = adbuf[n];
                else break;
            }
            m = hibyte(brut);
            adstat[n] = m;
            if (m==0XFF)
            {
                if (adtype[n]==adref)
                {
                    if (offset0!=m) ferror = 1; // switch/case if more than 1 device?
                }
                else ferror = 1;
                if (ferror)
                {
                    errnum = inbyte(brut, 3);
                    brut = 0;
                }
                p = m;
            }
            else p = adtype[n];
        }
        else p = adtype[n];
        if (p<5)
        {
            m &= 0X7F;     // COUNTING bit in MSB
            m >>= 4;
            if (p==m)
            {
                hibyte(brut) = 0;
                if (inbyte(brut, 1)&0X80) hibyte(brut) = -1;
                if (m==2) brut <<= 1;
                m = 1;
            }
            else
            {
                adstat[n] = 0;
                brut = 0;
                m = 0;
            }
        }
        else m = 1;
    }
    else m = 0;
    return (m);
}

void    refresh(uchar n)
{
    if (!tref)
    {
        adcom(n, adtype[n]);
        fresh = 1;
    }
}
