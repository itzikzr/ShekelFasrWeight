#include "fanbase.h"

uchar   startup()
{
    xdata uchar p[4], i;

    TR1 = 1;                    // system timer started
    EA = 1;                     // system interrupt enabled
    ET1 = 1;                    // clock interrupt enabled
//    puts("\nHELLO");
    falter = novload();
    fun = NULL;                 // pointer initialization
    comini(rsform);
    memcpy(p,novbuf+48,4);
    option = ConvertLongByte(p);
    memcpy(p,novbuf+52,4);
    option2 = ConvertLongByte(p);

    if (option2 & 0x400) puts("\n999999"); // op 51 opdcount
    else                 puts("\nHELLO");

    opol   = opbcd;
    bcd420 = opol;
    if (opbcd) bcdbuf = bcd(0 , 2);     // TRAN420 converter
    else {
        if (opvol) com420(0X1002);      // 0-5V voltage mode
        else       com420(0X1001);            // 4-20mA current mode
        bcdbuf = 0;                     // startup current
    }
/*
    opol   = opbcd;
    bcd420 = opol;
    bcdbuf = bcd(0 , 2);
*/
    wstop  = full0;
    fmerror = freqw  = fstab = fover = 0;
    ftzero = ftkeyp = fhold = 0;
    fngz = fdsneg = 0;
    fone   = funder = fzero = fnew = 0; 
    hibyte(wstop) = weight = ftest = 0;
    fstart = sprel = flock = frel8off = fdelay1 = frelsub8 = 0;
    ftare = indw = neww = 0;
    zref = zref2;
    spdata[0] = spdata0;
    spdata[1] = spdata1;
    spdata[2] = spdata2;
    spdata[3] = spdata3;
    spdata[4] = spdata4;
    spdata[5] = spdata5;
    
    if (inbyte(zref,2) & 0x80) hibyte(zref) = 0xff;
    else                       hibyte(zref) = 0;
    zref >>= 7;
    if (hibyte(zref)) hibyte(zref) = 0xff;
    if (!order) netto = gtotal = ptotal = 0;
    
    freboot = 1;
    if (!adini())
    {
        if (!errnum) errnum++;
        errors(errnum);
    }
    freboot = 0;

    fbcd = ftitle = fbat = 1;
    if (!EA) dummy() ;
    if (backl0 == 0) blon = 1;
    if (backl0 >  0) blon = tall = 0;
    backl = backl0 * 6;
    tbl = onesec;
    power_off_reset();
    delay(2000, 0);
    tforce = tforce0;
    fstatus = 1;
    fStartProfi = 1;
    fpatod = ftranstare = 0;
    olddisp = ftranzero = fmakez = 0;
    progvar = tdisplay  = 0;
    if (!odisp) display(0, disform);

    if (opdcount){
        for (i=8; i>0; i--){
            sprintf(secbuf,"%06ld",(ulong)(i*111111));
            puts(secbuf);
            if (kbhit()) break;
            pause(10);
        } 
        puts("\n000000");
    }   
    return (wakeup());
}

uchar   wakeup()
{
    uchar   n;

    fkey = 0;
    if (testbit(falter)){
        delay(3000, 1);
        if (kbhit()){
            n = 0;
            display(0, disform);
        }
        else n = scancode & 3;
    }
    else n = 1;

    if (atod(0));
    szero = zref1;
    if (!opgetze || ocount || drange0){ 
        getzer(0, 0);              // power up ZERO
        tare = szero = 0;
    }
    if (!calen && (n == 3 || n == 1)) n = 0;
    if (n) szero = 0; // test mode
    return (n);
}

void    dummy()
{
    discon(NULL);
    binstring(0,0);
    xflag(1, 0);
    gul2fan(0);
    SendIdent();
    puts("%%10752%%");
	getfac(0);
	countunit(0, facdisp);
}
