#include "fanbase.h"

ulong getfac(uchar device)
{
    uchar n;
    n = adtype[device];
    hibyte(brut) = Cfactor;
    if (adcom(device, adcnt))
    {
        adsamp = 0;
        while (!atod(device)) watchdog();
    }
    else brut = 0;
    adcom(device, n);
    adsamp = 0;
    errnum = 0;
    ferror = 0;
    return (brut);
}

bit  countmode(uchar device, uchar mode)
{
    uchar n;
    n = adtype[device];
    brut = 0;
    switch (mode)
    {
        case 0: hibyte(brut) = Cweight; break;
        case 1: hibyte(brut) = Ccount; break;
        case 2: hibyte(brut) = Cfix; break;
        default:;
        }
    mode = 1;
    if (hibyte(brut)) if (adcom(device, adcnt))
    {
        adsamp = 0;
        while (!atod(device)) watchdog();
    }
    else mode = 0;
    adcom(device, n);
    adsamp = 0;
    errnum = 0;
    ferror = 0;
    return (mode);
}

bit  countfac(uchar device, ulong fac)
{
    uchar n;
    n = adtype[device];
    brut = fac;
    if (adcom(device, adcnt))
    {
        adsamp = 0;
        while (!atod(device)) watchdog();
    }
    else fac = 0;
    adcom(device, n);
    adsamp = 0;
    errnum = 0;
    ferror = 0;
    return (fac);
}

bit  countunit(uchar device, float unit)
{
    unit = 1.0 / unit;
    unit *= fac2float(fac0);
    return (countfac(device, float2fac(unit)));
}

float  oneunit(ulong unit)
{
    facdisp = fac2float(unit);
    facdisp /= fac2float(fac0);
    facdisp = 1 / facdisp;
    return facdisp;
}

ulong getcount(uchar device, ulong csize)
{
    uchar n;
    n = adtype[device];
    brut = csize;
    if (brut) hibyte(brut) = Csample;
    else hibyte(brut) = Czero;
    if (adcom(device, adcnt))
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
        csize = errnum;
    }
    else csize = 6;
    adcom(device, n);
    adsamp = 0;
    errnum = (uchar) csize;
    if (errnum) ferror = 1;
    if (ferror) brut = 0;
    return (brut);
}

void UnitWeight()
{
    uchar r, f=10;
    if (!ocount) return;

    progvar = *(ulong*)(kunit);
    facdisp = oneunit(progvar);
    sprintf(ascbuf,"\n%f",facdisp);
    blink("\n1 UNIT", ascbuf, 10);
    r = getch(0);
    return; 
/******************************************************************/
    do{        
        switch (f){
            case 0: puts("\n0.00000"); break;
            case 1: puts("\n00.0000"); break;
            case 2: puts("\n000.000"); break;
            case 3: puts("\n0000.00"); break;
            case 4: puts("\n00000.0"); break;
            case 5: puts("\n0000000"); break;
        }
        if (kbhit()){
            r = getch(0);
            if (r == 'P') f++; 
            if (f > 5) f = 0;
            if (r == 'Z') break;
            if (r == 'T') return;;
        }
        watchdog();
    }while(1);
    if (f < 5) f += 129;
    else       f  = 0;

    dml = dmr = 1;
    progvar = getdata("\n UINT ", 0, f, 0);
    if (f > 0) f -= 129;
    facdisp = (float)labs(progvar) / fpow(10, f) ;
    if (countunit(0, facdisp)){
        progvar = getfac(0); 
        (ulong)*(kunit) = progvar;
        novsave();
    }
    dml = dmr = 0;
}

void Count()
{
    xdata uint  q = 0 ;
    xdata uint  m[] = { 0, 10, 20, 50, 100, 200, 500 } ;
    xdata uchar end = 0, f=0;

    clrscr()  ;
    dml = dmr = 1;
    puts("\n COUNT");
    while(!end){
        watchdog() ;
        switch(getch(0)){
            case 'P' : if (q){q = m[q]; end = 1;} break ;
            case 'T' : if (q == 0){dlb = dkg = 0; dpcs = 1;}
                       if (q <= 5) q++;  else q = 1;
                       printf("\n%6d", m[q]); break ;
            case 'Z' : q = 0; end = 1;
        }
    }
    if (q){
        puts("\nCLEAR "); 
        end = getch(0);
        puts("\n ==== ") ;

        if (tare){
            t1 = getare(0, 0X80000000);
            dtare = 0;
            z1 = getzer(0 ,1); 
        }

        progvar = getcount(0, 0); 
        sprintf(ascbuf,"\n%6d", q);
        blink("\n PUT  ", ascbuf, 10);
        end = getch(0);
        puts("\n ==== ") ;
        progvar = getcount(0, (ulong)q); 
//sprintf(ascbuf,"\n%ld",progvar); comstring(ascbuf); while(fsend);
        if (progvar > 0){
            (ulong)*(kunit) = progvar;
            novsave();
        }else{
            f = 1;
            if (!tare){
                progvar = *(ulong*)(kunit);
                if (!countfac(0, progvar)) f = 1;
            }
        }

        if (tare){
            progvar = zref0;
            zref0 = z1 - t1;  
            if (!adload(0)) f = 1;
            zref0 = progvar;
            tare  = getare(0, t1);
            progvar = *(ulong*)(kunit);
            if (!countfac(0, progvar)) f = 1;
            dpcs = 0;
        }
    }else f = 1;

    if (f){
        puts("\n ERROR");
        pause(10);
    }
    dml = dmr = 0;
}
