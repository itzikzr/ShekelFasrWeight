#include "fanbase.h"

void    cal12(void);
void    cal13(void);

void    calibz()
{
    dpcs = dkg = dlb = dbat = dzero = dtare = 0;
    if (comstat()) getcom();
    if (fun==NULL) fun = cal12;
    fun();
}


void    cal12()
{
    if (adtype[0] == zcal){
        if (atod(0)){
            if (testbit(ferror)){
            }
            else{
                fun = cal13;
                zref2 = zref0 = brut;
            }
        }
    }else{
        zref1 = 0;
        puts("\n------");
        inword(brut, 1) = loword(target0);
        inbyte(brut, 3) = 0;
        hibyte(brut) = inbyte(target0, 1);
        brut >>= 1;
        hibyte(brut) = zmode0;
        if (!adcom(0, zcal));
    }
    if (kbhit()) reboot();
}

void    cal13()
{
    sprintf(ascbuf,"%6d",(uint)offset0);
    blink("OFFSET", ascbuf, 10);
    atod(0);

    getch(0);

    temp = zref2;
    temp >>= 8;
    hiword(temp) = 0;
    temp -= 32767; 
    sprintf(ascbuf,"%6ld",temp);
    blink("ZREF 0", ascbuf, 10);

    fprog = 1; gosave();
}

