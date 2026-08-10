#include "fanbase.h"

bit     novread(uchar *s, uchar n)
{
    n <<= 3;
    n &= 0X78;
    timout = 3;
    do{
        icstop();
        watchdog();
    }
    while ((timout)&&(!icout(novrad)));
    if (timout){
        if (icout(n)){
            SCL = 1;
            n = icout(novrad+1);
        }
        else n = 0;
    }
    else n = 0;
    if (n){
        n = 0;
        while (n<7) s[n++] = icin(1);
        s[n] = icin(0);
    }
    icstop();
    return (n);
}

bit     novwrite(uchar *s, uchar n)
{
    n <<= 3;
    n &= 0X78;
    timout = 3;
    do{
        icstop();
        watchdog();
    }
    while ((timout)&&(!icout(novrad)));
    if (timout){
        if (icout(n)){
            for (n=0;n<8;n++){
                if (!icout(s[n])) n = 10;
            }
            if (n>8) n = 0;
        }
        else n = 0;
    }
    else n = 0;
    icstop();
    return (n);
}

bit     novload()
{
    uchar   n;

    icset(0);                   // requesting the I2C bus
    if (novread(novbuf, 0)){
        fnov = (novsig!=refsig);
        n = 1;
        while ((!fnov)&&(n<novsize)){
            if (novread(novbuf + (n<<3), n)) n++;
            else fnov = 1;
        }
    }
    else fnov = 1;
    fsync = 1;                      // releasing the I2C bus
//fnov = 1;
    if (fnov) 
        for (n=0;n<sizeof(novbuf);n++) novbuf[n] = novdef[n];
    return (~fnov);
}

void    novsave()
{
    uchar   n;

    novsig = refsig;
    icset(0);                       // requesting the I2C bus
    for (n=0;n<novsize;n++) novwrite(novbuf+(n<<3), n);
    fsync = 1;                      // releasing the I2C bus
    fnov = 0;
}

