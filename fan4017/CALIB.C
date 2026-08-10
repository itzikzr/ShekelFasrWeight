#include "fanbase.h"

void    cal0(void);
void    cal1(void);
void    cal2(void);
void    cal3(void);
void    cal4(void);
void    ShowTrimer(void);
void    calforce();

void    calib()
{
    dpcs = dkg = dlb = dbat = dzero = dtare = 0;
    if (comstat()) getcom();
    if (fun==NULL) fun = cal0;
    fun();
}

void    cal0()
{

    fnov = zref1 = 0;

    progvar = full1;
    hibyte(progvar) = 0;
    progvar = getdata("\n FULL ", progvar, disform, 0);
    if (progvar != full1) full1 = progvar;
    if (kprint) reboot();

    brut  = load0;
    hibyte(brut) = 0;
    brut = getdata(" LOAD ", brut, disform, 0);
    if (kprint) reboot();
    else{
        hibyte(brut) = ctemp0;
        load0 = brut;
        puts("\n CLEAR");
        fun = cal1;
    }
    if (kbhit()) reboot();
}

void    cal1()
{
    atod(0);
    if (kbhit()){
        if (getch(0)=='T') fun = cal2;
        else reboot();
    }
}

void    cal2()
{
    if (adtype[0] == zcal){
        if (atod(0)){
            if (testbit(ferror)){
                if (errnum) calforce();
            }
            else{
                fun = ShowTrimer;//cal3;
                zref2 = zref0 = brut;
            }
        }
    }
    else{
        puts("\n------");
        inword(brut, 1) = loword(target0);
        inbyte(brut, 3) = 0;
        hibyte(brut) = inbyte(target0, 1);
        brut >>= 1;
        hibyte(brut) = zmode0;
        if (!adcom(0, zcal)) errors(1);
    }
    if (kbhit()) reboot();
}

void    ShowTrimer()
{
    if (testbit(ferror)) errors(errnum);
    timeout = 15;
    while(timeout){
        altern("\nOFFSET", (ulong)offset0, 0);
        watchdog();
    }
    talter = 0;
    fun = cal3;
}

void    cal3()
{
    atod(0);
    brut = load0;
    hibyte(brut) = 0;
    altern("  PUT ", brut, disform);
    if (kbhit()){
        if (getch(0)=='T') fun = cal4;
        else reboot();
    }
    if (testbit(ferror)) errors(errnum);
}

void    cal4()
{
    xdata float f, l;
    xdata uchar n;

    if (adtype[0] == wcal){
        if (atod(0)){
            if (testbit(ferror)){
                if (errnum) calforce();
            }
            else{
                fac0 = brut;
                n = DecimalPointNum(disform);
                switch(n){
                    case 0: l = fac2float(fac0-0X3000000) * 1000; break;
                    case 1: l = fac2float(fac0-0X3000000) * 100 ; break;
                    case 2: l = fac2float(fac0-0X3000000) * 10  ; break;
                    case 3: l = fac2float(fac0-0X3000000)       ; break;
                    case 4: l = fac2float(fac0-0X3000000) / 10  ; break;
                    case 5: l = fac2float(fac0-0X3000000) / 100 ; break;
                }
                if (full1) f = (l / ((float)full1 / (float)(fpow(10,n)*10)));
                else       f =  l;
                printf("\n%7.5f", f);
                if (getch(0)=='P') reboot();
                fprog = fsave = 1;
                gosave();
            }
        }
    }
    else{
        puts("\n  CAL ");
        brut = load0;
        if (!adcom(0, wcal)) errors(1);
    }
//    if (kbhit()) reboot();
    if (testbit(ferror)) errors(errnum);
}

void    calforce()
{
    if (errnum == 6) errdisp(errnum);
    else errors(errnum);
    do {
        rsbyte = getch(0);
        if (rsbyte) examine(adbuf[0]);
    } while (rsbyte == 'S');
    if (rsbyte == 'T') timout = forsec;
    else timout = 0;
    while ((timout) && (ktare)) {
        watchdog();
        if (comstat()) getcom();
    }
    if ((ktare)||(rsbyte=='C')) adtype[0] = 0;
    else reboot();
}
void    home()
{
    if (!adload(0)) errors(1);
    clrscr();
    fun = NULL;
    prog = falter = fkey = 0;
}

void    gosave()
{
    home();
    puts("\n SAVE ");
    fun = psave;
    prog = 3;
}

void    psave()
{
    if (kbhit()){
        if (getch(0) == 'T'){
            if (fprog){  
                if (!(novop2 & 0x00004)) auto0 = 0; //op 35
                else                     auto0 = 0X20;

                if (novop2 & 0x100000) op0 |= 0x02; //op 53
                else                   op0 &= 0Xfd;

                novsave();
                puts("\n DONE ");
                pause(10);
                reboot();
            }
        }
        reboot();
    }
}

void    errdisp(uchar n)
{
    fsync = 1;
    flcd  = 1;
    tbat  = tensec;
    printf("\nERR%3u", (uint) n);
    terrin = onesec;
}

void    errors(uchar n)
{
    if (n) {
        errdisp(n);
        while (terrin) {
            if (oprofi){
                senderror(n);
                fmerror = 1;
            }
            if (kbhit()){
                if (freboot){
                    fmerror = 1;
                    return;
                }else reboot();
            }
            else         watchdog();

            if (!osetpoint) {
                if (oconti) SendWeightToCom(brut);
                else if (comstat()) serial();
            }

            if ((n>3)||(errnum)) terrin = onesec;
            if (n<4) {
                ferror = 0;
                if (atod(0)) {
                    if (!ferror) errnum = 0;
                } else if (!terrout) {
                    if (adtype[0]>adwey) adcom(0, adraw);
                    else adcom(0, adwey);
                    terrout = onesec;
                }
            }
        }
    }
}

