#include "fanbase.h"

bit    status(uchar s)
{
    xdata uchar   k, f;

    if (adtype[0]==adraw)
    {
        dzero = 0;
        return (0);
    }

    k = adstat[s];

    if (k & 0X80 && ocount){       // COUNTING mode flag
        f = dpcs = 1;
        k &= 0X7F;
    }else f = dpcs = 0;        // end of COUNTING detection

    if (k & 0x01) fstab = 0;
    else          fstab = 1;

    if ((k & 0x02) || (brut >= wstop && !hibyte(brut) && !dpcs)) fover = 1;
    else                                                         fover = 0;

    if (k & 0x04) funder = 1;
    else          funder = 0;
    
    if (opgetze){
        if (((k & 0x08) || (weight0 && weight0 == szero))) fzero = dzero = 1;
        else                                               fzero = dzero = 0;
    }else{
        if (k & 0x08) fzero = dzero = 1;
        else          fzero = dzero = 0;
    }
/*
    if (!fone && ((k & 0x08) || (!brut && !tare) || (weight0 == szero))) fzero = dzero = 1;
    else                                                                 fzero = dzero = 0;
*/
    if (ostab){
        if (fstab){
            if (f) dpcs = 1;
            else{
                if (okilo ) dkg = 1;
                if (opound) dlb = 1;
            }
        }else{
            if (f) dpcs = 0;
            else{
                if (okilo ) dkg = 0;
                if (opound) dlb = 0;
            }
        }    
    }else{
        if (f) dpcs = 1;
        else{
            if (okilo ) dkg = 1;
            if (opound) dlb = 1;
        }
    }

    if (prog) return 0;

    if (funder || ((weight0+wtare(tare)) < szero)){
//sprintf(ascbuf,"\n%ld + %ld < %ld",wtare(tare),weight0,szero); comstring(ascbuf); while(fsend);
        if (oneg) funder0 = 1;
        if (oneg) puts("\n------");
        if (opbigdsp) xputs();
        if (zerind >= 5 && (fstab || onostabTZ) && otoneg) treqz = trisec;
        else                                               zerind++;
        return 1;
    }else{
        if (fmakez){
            progvar = brut;
            remove_weight();
            novsave();
            brut = progvar;
//sprintf(ascbuf,"\n1 %ld %ld %ld %ld",zref1,temp,szero); comstring(ascbuf); while(fsend);
        }
        fmakez = funder0 = zerind = 0;
    }

    if (fover){
        puts("\n STOP ");
        if (opbigdsp) xputs();
        return 1;
    }
    return 0;
}

ulong   getzer(uchar device, uchar mode)
{
    uchar   n;

    n = adtype[device];
    if (adcom(device, adzer)){
        adsamp = 0;
        errnum = 0;
        do {
            watchdog();
            if (comstat()) serial();
            if (kbhit()) break;
            ferror = 0;
            if (atod(device)){
                if ((errnum)||(!mode)) ferror = 0;
            }
            else ferror = 1;
        }
        while (ferror);
    }
    if (errnum == 6) mode = 0;
    else             mode = errnum;
//sprintf(ascbuf,"\n33 %d %d %d %d",(uint)treqz,(uint)fstab,(uint)tstab,(uint)onostabTZ); comstring(ascbuf); while(fsend);
    adcom(device, n);
    adsamp = 0;
    if (mode){
        errnum = mode;
        ferror = 1;
    }
    if (ferror) brut = 0;
    return (brut);
}

//  fan2gul - Converts long to int from guliver style A to D to fantom one.
 
uint fan2gul(long n)
{
    n += 0x1000080L;
    n >>= 9;
    return((uint)n);
}

long gul2fan(uint n)
{
    xdata long x;

    x = (long)n;
    x <<= 9;
    x -= 0x1000000L;
    return(x);
}

ulong getare(uchar device, ulong tare)
{
    temp = adtype[device];
    brut = tare;
    if (adcom(device, adtar))
    {
        adsamp = 0;
        errnum = 0;
        do
        {
            watchdog();
            if (comstat()) break;
            if (kbhit()) break;
            ferror = 0;
            if (atod(device))
            {
                if (errnum) break;
            }
            else ferror = 1;
        }
        while (ferror);
        tare = errnum;
    }
    else tare = 0;
    adcom(device, temp);
    adsamp = 0;
    errnum = (uchar) tare;
    if (errnum) ferror = 1;
    if (ferror) tare = 0;
    else        tare = brut;
    dtare = tare;
//sprintf(ascbuf,"\n2 %ld",tare); comstring(ascbuf); while(fsend);
    return (tare);
}

ulong tareweight(uchar device, float tweight)
{
    temp = (ulong) (tweight * 128.0 / fac2float(fac0));
    temp = getare(device, temp);
    return (temp);
}

ulong tround(ulong w);
ulong wtare(ulong t)
{
    t *= fac2float(fac0);
	t += 64;
    t /= 128;
	//t += (128>>1);
	//t >>= 7;
    t = tround(t);
    return t;
}

ulong tround(ulong w)
{
//    sherit = wround >> 1;
    w += (round0 >> 1);
    w /= round0;
    w *= round0;
    return w;
}
