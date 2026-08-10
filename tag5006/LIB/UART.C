
#include "tigerl.inc"
#include "tigerl.h"

void    comini(uchar u) small
{
      u ^= 0X10;
      fbit = u & 0X40;
      fdis = u & 0X10;
      fodd = u & 0X20;
      TH1 = ubaud[u&7];			// Baud rate
      TL1 = TH1;
      TR1 = 1;          		// Start timer
      if ((u&0x50)==0x40) SCON = 0XDC ; // UART control
      else SCON = 0x5C ;
      receive = (uint) inbuf;
      ES = 1;           		// Interrupt enable
      flush();
}

void    flush() small
{
      uartin = receive;
}

bit   comstat() small
{
      return (uartin-receive);
}

uchar getcom() small
{
      uchar     k;
      while ((!comstat())&&(!kbhit())) watchdog();
      if (comstat()) {
          k = * (uchar *) uartin;
          if ((++uartin)==((uint) bufend)) uartin = (uint) inbuf;
      }
      else k = 0;
      return (k);
}

void	putcom(uchar k) small
{
        ES = 0 ;
        * ((uchar xdata *) receive) = k ;
	if ((++receive) == (uint) bufend) receive = (uint) inbuf ;
	ES = 1 ;
}

void    comstring(uchar *s) small
{
  while (fsend) watchdog();
  byte(transmit, 0) = byte(s, 2);
  byte(transmit, 1) = byte(s, 1);
  rsmode &= 0XF0;
  rsmode |= (byte(s, 0)&7);
  T1 = 0 ;
  TI = 1 ;
  ES = 1 ;
}

void    binstring(uchar *s, uint n) small
{
  while (fsend) watchdog();
  byte(transmit, 0) = byte(s, 2);
  byte(transmit, 1) = byte(s, 1);
  rsmode &= 0XF0;
  rsmode |= (byte(s, 0)&7) | 8;
  ucount = n;
  T1 = 0 ;
  TI = 1 ;
  ES = 1 ;
}

void  xcomini(uchar u) small
{
      xuartc = 0XFE;
      xuartc = xout;
      xuartd = ((u&0X70)|3);
      xuartd = 0XC0;
      xuartd = xbaud[u&7];
      xuartc = xout;
      xreceive = (uint) xinbuf;
      xflush();
      EX0 = 1;
}

void  xflush() small
{
      xinbuf[0] = xuartd;
      xuartin = xreceive;
}

bit   xcomstat() small
{
      return (xuartin-xreceive);
}

uchar xgetcom() small
{
      uchar     k;
      while ((!xcomstat())&&(!kbhit())) watchdog();
      if (xcomstat()) {
          k = * (uchar *) xuartin;
          if ((++xuartin)==((uint) xbufend)) xuartin = (uint) xinbuf;
      }
      else k = 0;
      return (k);
}

void  xputcom(uchar k) small
{
      EX0 = 0;
      * ((uchar xdata *) xreceive) = k;
      if ((++xreceive)==((uint) xbufend)) xreceive = (uint) xinbuf;
      EX0 = 1;
}

void    xcomstring(uchar *s) small
{
  while (fxsend) watchdog();
  byte(xtransmit, 0) = byte(s, 2);
  byte(xtransmit, 1) = byte(s, 1);
  rsmode &= 0XF;
  rsmode |= (byte(s, 0)&7) << 4;
  xuartc = xmode;
}

void    xbinstring(uchar *s, uint n) small
{
  while (fxsend) watchdog();
  byte(xtransmit, 0) = byte(s, 2);
  byte(xtransmit, 1) = byte(s, 1);
  xcount = n;
  rsmode &= 0XF;
  rsmode |= (byte(s, 0)&7) << 4;
  rsmode |= 0X80;
  xuartc = xmode;
}

uchar allstat() small
{
      if (uartin-receive) return (1);
      if (xuartin-xreceive) return (2);
      if (kbhit()) return (3);
      return (0);
}

uint    allcom() small
{
      uint      k;
      while (!(k=allstat())) watchdog();
      byte(k, 0) = byte(k, 1);
      switch (byte(k, 0)) {
          case 1: byte(k, 1) = * (uchar *) uartin;
                  if ((++uartin)==((uint) bufend)) uartin = (uint) inbuf;
                  break;
          case 2: byte(k, 1) = * (uchar *) xuartin;
                  if ((++xuartin)==((uint) xbufend)) xuartin = (uint) xinbuf;
                  break;
          default: byte(k, 1) = getch();
      }
      return (k);
}
