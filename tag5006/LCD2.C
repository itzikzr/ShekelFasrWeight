
#include "tigerl.inc"
#include "tigerl.h"

#define lcdcom8(x)      lcdbit8(0, x); timelcd = 3; while(timelcd); watchdog()
#define lcdcom4(x)      lcdbit4(0, x); timelcd = 3; while(timelcd); watchdog()

uchar   lcdbit8(uchar lctype, uchar lcdat) small
{
/*
        interface data lenght is 8 bits
*/

        if (lctype) bimage |= 0x10;
        else bimage &= 0xef;
        lcdtran(lcdat&0x0f) ;      // 4 LSBits of lcdat
        return (lcdat) ;
}
uchar   lcdbit4(uchar lctype, uchar lcdat) small
{
/*
        interface data lenght is 4 bits
*/
        uchar   sport ;
        if (lctype) bimage |= 0x10;
        else bimage &= 0xef;
        lcdtran(lcdat>>4) ;        // 4 MSBits of lcdat

        lcdtran(lcdat&0x0f) ;      // 4 LSBits of lcdat

        return (lcdat) ;
}

void    lcdini(uchar type) small
{
        lcdcom8(0X03);
        lcdcom8(0X03);
        lcdcom8(0X03);
        lcdcom8(0X02);
        lcdcom4(0X28);
        if (type==0) {
            lcdcom4(8);
            lcdcom4(1);
        }
        lcdcom4(6);
        lcdcom4(0XC);
}

void    lcdputs(uchar x, uchar y, uchar *s)
{
        uchar k ;

	k = 0x40*(y-1) + (x-1) ;
//	lcdbit4(0, k|0x80) ;
	lcdcom4(k|0x80);
        while(*s)  lcdbit4(1, *s++) ;
}

void    lcdstart(void)
{
        lcdputs(1, 1, "     SHEKEL     ") ;
        lcdputs(1, 2, "Electronic Scale") ;
}

void    chargen(uchar cg, uchar bmp[]) small
{
        uchar k;

        cg &= 7;
        cg <<= 3;
        lcdbit4(0, cg|0X40);
        for (k=0;k<8;k++) lcdbit4(1, bmp[k]);
}

uchar   *htrans(uchar *s)               // hebrew translation
{
/*
        "€‚ƒ„…†‡ˆ‰ ‹ŒŽ "              // offset 0xA
        "‘’ ” – ˜™š     "
        "“  •           "              // offset 0xE
        "    —   Š       "
*/
        code uchar lcdtab[] = {
        "€‚ƒ„…†‡ˆ‰ ‹ŒŽ ‘’ ” – ˜™š     “  •               —   Š       "
        } ;
        uchar d[80], t, i = 0 ;

        while(s[i]) {
            if ((s[i] >='€')&&(s[i] <='š')) {
                for (t = 0 ; lcdtab[t] ; t++) {
                    if (s[i] == lcdtab[t]) {
                        if (t < 32) d[i] = t + 0xA0 ;
                        else  d[i] = t + 0xC0 ;
                        break ;
                    }
                }
            }
            else d[i] = s[i] ;
            i++ ;
        }
        d[i] = 0 ;
        return d ;
}

//*******************
uchar   *htrans_new(uchar *s)           // hebrew translation
{
        code uchar lcdtab[] = {
        "€‚ƒ„…†‡ˆ‰Š‹ŒŽ‘’“”•–—˜™š"
        } ;
        uchar d[80], t, i = 0 ;

        while(s[i]) {
            if ((s[i] >='€')&&(s[i] <='š')) {
                for (t = 0 ; lcdtab[t] ; t++) {
                    if (s[i] == lcdtab[t]) {
                        if (t < 28) d[i] = t + 0xA0 ;
                        else  d[i] = t + 0xC0 ;
                        break ;
                    }
                }
            }
            else d[i] = s[i] ;
            i++ ;
        }
        d[i] = 0 ;
        return d ;
}
//*******************