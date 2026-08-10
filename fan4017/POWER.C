#include "fanbase.h"

void    pwroff()
{
    EX1 = 0;
    clrscr();
    pcount = biostime(200);
    while (1)
    {
        watchdog();
        if (!INT1) pcount = biostime(200);
        if (btm(pcount)) PWR = 1;
    }
}

uint    getbat(uchar device)
{
    uchar n;
    uint  m;

    n = adtype[device];
    if (adcom(device, adbat)){
        adsamp = 0;
        while (!atod(device)) watchdog();
    }else brut = 0;
    m = loword(brut);
    adcom(device, n);
    adsamp = 0;
    errnum = 0;
    ferror = 0;
    while(!atod(0)) watchdog();
    return (m);
}

void power_off()
{
    if (!poff) return;
    if (!tpoff){ 
        poffcounter++;
        tpoff = onesec;
    }
    if (poffcounter >= (poff * 60)) pwroff();
}

void power_off_reset()
{
    poffcounter = 0;
    tpoff = onesec;
}

void check_bat(uint n)
{
    uint h,l;

    switch(batisp){
        case 1 : h = 195; l = 189; break; // 12v 195=10.8v 189=10.5v
        case 2 : h = 138; l = 0;   break; // 9v  135=7.5v
        case 3 : h = 97 ; l = 93;  break; // 6v  97=5.4v 93=5.2v
    }  
    n &= 0x00ff;
//sprintf(ascbuf,"\nbat= %3d  hi= %3d  lo= %3d  %d",n , h , l,(uint)batisp); comstring(ascbuf); while(fsend) watchdog();
    if (fbat1){
        if (n <= l){ 
            puts("\nLOW.BAT"); 
            pause(30); pwroff();
        }else{
            if (n <= h) dbat = 1;
            else        dbat = 0;
        }
    }
    if (n != oldbat) fbat1 = 1;
    else             fbat1 = 0;
    oldbat = n;
    
}

void    reboot()
{
    adcom(0, adset);
    delay(300, 0);
    tatod = onesec;
    adsamp = 0;
    while (tatod) {
        if (atod(0)) break;
        watchdog();
    }
    fun = NULL;
    fun();
}
