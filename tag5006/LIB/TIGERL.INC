#pragma      debug
#pragma      symbols
#pragma      large
#pragma      nointpromote
#pragma      nointvector
#pragma      objectextend
#pragma      noprint

#include     <reg51.h>
#include     <intrins.h>
#include     <absacc.h>
#include     <stdio.h>
#include     <stdarg.h>
#include     <stdlib.h>
#include     <string.h>

#define uchar unsigned char
#define uint  unsigned int
#define ulong unsigned long

#define nop             _nop_()
#define nop2            _nop_();_nop_()
#define nop3            _nop_();_nop_();_nop_()
#define nop4            _nop_();_nop_();_nop_();_nop_()
#define testbit         _testbit_
#define byte(n, m)      *(((uchar *) (&n)) + m)
#define word(n, m)      *((uint *) (((uchar *) (&n)) + m))
#define quad(n, m)      *((ulong *) (((uchar *) (&n)) + m))
#define real(n, m)      *((float *) (((uchar *) (&n)) + m))
#define outportb(port,byte) port = byte
#define inportb(port,byte) byte = port

#define timeout timers[1]
#define timelcd fastimers[6]
