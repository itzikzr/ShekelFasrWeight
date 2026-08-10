#include "fanbase.h"

void    test0(void);
void    test1(void);
void    test2(void);
//void    test23(void);
void    test3(void);
void    show_fac(void);
void    test4(void);

void    testmode()
{
    ferror = 0;
    if (comstat()) getcom();
    if (atod(0))
    {
        if (fun==NULL) fun = test0;
        fun();
    }
    if (kbhit()) testkey(getch(0));
}

void    test0()
{
    adcom(0, adwey);
    brut = 0;
    adcom(0, adlim);                    // remove zero limits
    adcom(0, adraw);
    dmr = dml = 1;
    dur = dul = dlr = dll = dpcs = 0;
    fun = test1;
}

void    test1()
{
//sprintf(ascbuf,"\n%ld",brut); comstring(ascbuf); while(fsend);
    switch (errnum)
    {
        case 2:  puts("\nERR  2"); break ;
        case 3:  puts("\nERR  3"); break ;
        default: status(0);
                 atod2weight(brut, 0);
    }
    errnum = 0;
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
        case 2:  puts("\nERR  2"); break ;
        case 3:  puts("\nERR  3"); break ;
        default: display(fan2gul(brut), 0);
    }
    errnum = 0;
    if (testbit(falter)) fun = test3;//23;
}
/*
void    test23()
{
    dmr = dur = dlr = 0;
    dul = dll = dml = 1;
    dkg = dlb = dzero = dpcs = 0;
    switch (errnum)
    {
        case 2:  puts("\nERR  2"); break ;
        case 3:  puts("\nERR  3"); break ;
        default: errnum = 0;
                 brut += 0X80;
                 brut >>= 8;
                 if (inbyte(brut, 1)) hibyte(brut)--;
                 display(brut, 0);
    }
    errnum = 0;
    if (testbit(falter)) fun = test3;
}
*/
void    test3()
{
    xdata uint  n;

    dur = dlr = dul = dll = 0;
    dkg = dlb = dzero = dpcs = 0;
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
        case 0: f = fac2float(fac0-0X3000000) * 1000; break;
        case 1: f = fac2float(fac0-0X3000000) * 100 ; break;
        case 2: f = fac2float(fac0-0X3000000) * 10  ; break;
        case 3: f = fac2float(fac0-0X3000000)       ; break;
        case 4: f = fac2float(fac0-0X3000000) / 10  ; break;
        case 5: f = fac2float(fac0-0X3000000) / 100 ; break;
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
/*
float fac2float(ulong n)
{
    uchar m;
    m = 134 - hibyte(n);
    do
    {
        n <<= 1;
        m--;
        if (hibyte(n)&1) break;
    }
    while (m);
    hibyte(n) = m;
    n >>= 1;
    inbyte(facdisp, 0) = inbyte(n, 3); // IEEE float conversion with
    inbyte(facdisp, 1) = inbyte(n, 2); // Keil-Franklin inconsistency
    inbyte(facdisp, 2) = inbyte(n, 1); // Long is big endian
    inbyte(facdisp, 3) = inbyte(n, 0); // while Float is little endian
    return (facdisp);
}

ulong float2fac(float f)
{
    inbyte(temp, 0) = inbyte(f, 3);  // IEEE float conversion with
    inbyte(temp, 1) = inbyte(f, 2);  // Keil-Franklin inconsistency
    inbyte(temp, 2) = inbyte(f, 1);  // Long is big endian
    inbyte(temp, 3) = inbyte(f, 0);  // while Float is little endian
    temp <<= 1;
    hibyte(temp) = 133 - hibyte(temp);
    hibyte(temp) <<= 1;
    hibyte(temp)++;
    temp >>= 1;
    return (temp);
}
*/

float fac2float(ulong n)
{
    uchar m;
    m = 134 - hibyte(n);
    do
    {
        n <<= 1;
        m--;
        if (hibyte(n)&1) break;
    }
    while (m);
    hibyte(n) = m;
    n >>= 1;
#if __C51__ < 500
	inbyte(n, 3) ^= inbyte(n, 0);		// float is big endian from version 5.00 and up
	inbyte(n, 0) ^= inbyte(n, 3);
	inbyte(n, 3) ^= inbyte(n, 0);
	inbyte(n, 2) ^= inbyte(n, 1);
	inbyte(n, 1) ^= inbyte(n, 2);
	inbyte(n, 2) ^= inbyte(n, 1);
#endif
	return(infloat(n, 0));
}

ulong float2fac(float f)
{
#if __C51__ < 500
	inbyte(f, 3) ^= inbyte(f, 0);		// float is big endian from version 5.00 and up
	inbyte(f, 0) ^= inbyte(f, 3);
	inbyte(f, 3) ^= inbyte(f, 0);
	inbyte(f, 2) ^= inbyte(f, 1);
	inbyte(f, 1) ^= inbyte(f, 2);
	inbyte(f, 2) ^= inbyte(f, 1);
#endif
    inlong(f, 0) <<= 1;
	hibyte(f) = 133 - hibyte(f);
	hibyte(f) <<= 1;
	hibyte(f)++;
	return (inlong(f, 0) >> 1);
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
        case 'P': reboot();
    }
}

float   bodyfac(ulong n);
void  atod2weight(long avg, uchar n)
{
    xdata float x;
    xdata ulong z;
    xdata uchar f;

    z = zref2; z <<= 1;
    if (hibyte(z) & 1) hibyte(z) = 0xff;
    else               hibyte(z) = 0;
    f = inbyte(avg, 1); 

    avg = fan2gul(avg);
    avg -= (long)fan2gul(z);
    if (kzero) progvar = avg;
    avg -= progvar;
//    if (kzero) szero = avg;
//    avg -= szero;

    x = avg;
    x *= bodyfac(fac0);
 
    if (!n){
        if (f == 0x01) puts("\n STOP ");
        else if (f == 0xfe) puts("\n------");
             else display(x, disform);
    }
}
/*
float   bodyfac(ulong n)
{
    float   x;

    hibyte(x) = 135 - hibyte(n);
    hibyte(n) = 0;
    if (n){
        while (!hibyte(n)){
            n <<= 1;
            hibyte(x)--;
        }
        hibyte(n) = hibyte(x);
        n >>= 1;
        inbyte(x, 0) = inbyte(n, 3);    // IEEE float conversion with
        inbyte(x, 1) = inbyte(n, 2);    // Keil-Franklin inconsistency
        inbyte(x, 2) = inbyte(n, 1);    // Long is big endian
        inbyte(x, 3) = inbyte(n, 0);    // while Float is little endian
    }else x = 0;
    return (x);
}
*/
float   bodyfac(ulong n)
{
	uchar	m;

	m = 135 - hibyte(n);
    hibyte(n) = 0;
    if (n) {
        while (!hibyte(n)){
            n <<= 1;
			m--;
        }
        hibyte(n) = m;
        n >>= 1;
#if __C51__ < 500
		inbyte(n, 3) ^= inbyte(n, 0);		// float is big endian from version 5.00 and up
		inbyte(n, 0) ^= inbyte(n, 3);
		inbyte(n, 3) ^= inbyte(n, 0);
		inbyte(n, 2) ^= inbyte(n, 1);
		inbyte(n, 1) ^= inbyte(n, 2);
		inbyte(n, 2) ^= inbyte(n, 1);
#endif
    } else n = 0;
	return(infloat(n, 0));
}
