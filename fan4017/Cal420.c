#include "fanbase.h"


void com420(uint n)
{
    bcdbuf = n;
    timeout = 3;
    while (timeout) watchdog();
}

void calib420()    // 4-20mA keyboard calibration  Function 11
{
    xdata uint m, n;

    if (opbcd) return;
    com420(0X1001);    // set current mode
    
    bcdbuf = 0XA000;   // 6mA calibration
    m = (uint) getdata("\nCAL  6", 6000L, 0X83, 7000L);
    bcdbuf = 0XE000;   // 18mA calibration

    n = (uint) getdata("\nCAL 18", 18000L, 0X83, 19000L);

    progvar = param420(m, n);

    hiword(progvar) -= CFACBIAS;
    loword(progvar) -= COFFBIAS;
    if (inbyte(progvar, 2) & 0XE0) hibyte(progvar) = -1;
    if (hibyte(progvar) & 0XE0)
    {
        oconti = 0;
//        errors(11);
        puts("\nOUT.REN");
        n = getch(0);
    }else{
        puts("\n DONE ");
        hibyte(progvar) |= 0X60;
        inbyte(progvar, 2) |= 0X40;
        com420(hiword(progvar));

        com420(loword(progvar));

        bcdbuf = 0X1003;
        timeout = onesec;
        while (timeout) watchdog();
        bcdbuf = 0;
    }
}

ulong param420(uint I6, uint I18)
/*
 I6 is the observed current for a nominal value of 6mA
 I18 is the observed current for a nominal value of 18mA
 The high word of the returned value is the new conversion factor
 The low word of the returned value is the new conversion offset
*/
{
    xdata ulong n;
    xdata float f;

    I18 -= I6;
    inlong(f, 0) = 0X4DFA0EA8;
    f /= I18;
    hiword(n) = (uint) (f + 0.5);
    f /= 16003.9072;
    I6 -= 4000;
    f *= I6;
    f = -f;
    f += 16384.25;
    loword(n) = (uint) f;
    watchdog();
    return (n);
}

void calib05()    // 0-5V keyboard calibration  Function 12
{
    xdata uint m, n;

    if (opbcd) return;

    com420(0X1002);    // set voltage mode
    bcdbuf = 0X9999;   // 1V calibration (0.999923705)

  //m = (uint) getdata("\nCAL  1", 10000L, 0X82, 20000L);
    m = (uint) getdata("\nCAL  1", 1000L , 0X83, 2000L) * 10;
    bcdbuf = 0XE666;   // 4V calibration (4.0)
  //n = (uint) getdata("\nCAL  4", 40000L,0X82, 50000L);
    n = (uint) getdata("\nCAL  4", 4000L ,0X83, 5000L) * 10;

    progvar = param05(m, n);
    hiword(progvar) -= VFACBIAS;
    loword(progvar) += VOFFBIAS;
    if (inbyte(progvar, 2) & 0XF0) hibyte(progvar) = -1;
    if (hibyte(progvar) & 0XF0)
    {
        oconti = 0;
        //errors(11);
        puts("\nOUT.REN");
        n = getch(0);
    }
    puts("\n DONE ");
    hibyte(progvar) |= 0X30;
    inbyte(progvar, 2) |= 0X20;
    com420(hiword(progvar));
    com420(loword(progvar));
    bcdbuf = 0X1003;
    timeout = onesec;
    while (timeout) watchdog();
    bcdbuf = 0;
}
/*
void look(uchar *s, uint n)
{
    while (fsend) watchdog();
    sprintf(ascbuf,"\n %s = %u (%04X) ", s, n, n);
    comstring(ascbuf);
}
*/
ulong param05(uint V1, uint V4)
/*
 V1 is the observed voltage for a nominal value of 1V
 V4 is the observed voltage for a nominal value of 4V
 The high word of the returned value is the new conversion factor
 The low word of the returned value is the new conversion offset
*/
{
    xdata ulong n;
    xdata float f;

    V1 = V4 - V1;
    inlong(f, 0) = 0X4CEA6187;
    f /= V1;
    hiword(n) = (uint) (f + 0.5);
    f *= V4;
    f *= 1.3107;
    hibyte(f) -= 0X86;
    f += 52428;
    loword(n) = (uint) f;
    return (n);
}
