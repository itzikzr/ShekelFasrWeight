
#include "tigerl.inc"
#include "tigerl.h"

/*
            ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
            ³                                          ³Û
            ³    I2C bus generalized device handling   ³Û
            ³                                          ³Û
            ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

uchar   icbyte(uchar) small;
bit     icack(bit);

uchar   icbyte(uchar k) small
//      byte transfer between k and the device
{
        uchar   i;
        for (i=0;i<8;i++) {
	    sda = (bit) (k&0X80);
	    nop4;
            scl = 1;
            k <<= 1;
            k += (uchar) sda;
            scl = 0;
        }
        return (k);
}

bit     icack(bit f)
//      bit acknowledge function
{
        sda = f;
        scl = 1;
        nop4;
        f = sda;
        scl = 0;
        return (f);
}

bit     open(uchar device, uchar address)
//      device opening at a given address
{
        timeout = 5;
        while (timeout) {
           scl = 0;
           nop4;
           sda = 1;
           nop4;
           scl = 1;
           nop4;
           sda = 0;
           nop4;
           scl = 0;
           icbyte(device);
           if (!icack(1)) break;
        }
        if (!timeout) return (0);
        icbyte(address);
        return (~icack(1));
}

void    close()
//      closing any open device (STOP bit)
{
        scl = 0;
        nop4;
        sda = 0;
        nop4;
        scl = 1;
        nop4;
        sda = 1;
}

void    icini()
//      I2C bus initialization by closing all devices
{
        uchar   i;
        for (i=0;i<9;i++) close();
}

bit     write(uchar n) small
//      writing n bytes to a device open for writing
{
        uchar   i;
        i = 0;
        while (i<n) {
            icbyte(icbuf[i]);
            if (!icack(1)) i++;
            else break;
        }
        return (i==n);
}

bit     read(uchar device, uchar n) small
//      setting an open device to read mode, and reading n bytes
{
        uchar   i;
        sda = 1;        // restarting
        nop4;
        scl = 1;
        nop4;
        sda = 0;
        nop4;
        scl = 0;
        icbyte(device|1);
        if (icack(1)) return (0);
        i = 0;
        while (i<n) {
            icbuf[i++] = icbyte(0XFF);
            if (i<n) icack(0);
        }
        return (1);
}
