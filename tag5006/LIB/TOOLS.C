#include "tigerl.inc"
#include "tigerl.h"
/*
   ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
   ³           Toolkit of general purpose functions           ³Û
   ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/
/* * * * * * * * * * * * * * * * * * * * * * * */
#define twait timers[15]        // expection timer
void    pause(uchar  time)
{
        twait = time ; while((twait)&&(!kbhit())) watchdog();
}
/* * * * * * * * * * * * * * * * * * * * * * * */
bit     getbitport(uchar n)     /* porta - read only port */
{
        uchar d = 1 ;

        return(porta&(d<<n)) ;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
void    message(uchar  *s, uchar t)
{
        uchar   d[8] = {"      "} ;
        uchar   m[100], *p ;

        bell(2) ; printf("\1%s", d) ;
        sprintf(m, "%s%s%s", d, s, d) ;
        p = m + 5 ;
        while(*p++) {
           if (kbhit())   break ;
           memmove(d, p - 6, 6) ; printf("\1%s", d) ; pause(t) ;
        }
}
/* * * * * * * * * * * * * * * * * * * * * * * */
float   max(float a, float b)
{
        if (a > b)  return(a) ;
        else        return(b) ;
}
float   min(float a, float b)
{
        if (a < b)  return(a) ;
        else        return(b) ;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
void    strspace(uchar *s, int n)
{
        memset(s, ' ', n) ; s[n] = 0 ;
}
bit     is_space(uchar *s)
{
        while(*s) if (*s++ != ' ') return(0) ;
        return(1) ;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
uchar  *trim(uchar *s, uchar side)
{
        uchar  t ;

        if (side != right) {
            t = 0 ; while(s[t] == ' ') t++ ;
            strcpy(s, s+t) ;
        }
        if (side != left) {
            t = strlen(s)-1 ; while(s[t] == ' ') t-- ;
            s[t+1] = 0 ;
        }
        return(s) ;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
float   fpow(float b, int e)    // expanenta calculation
{
        float  bas = 1 ;

        if (e < 0) while(e++) bas /= b ;
        else       while(e--) bas *= b ;
        return(bas) ;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
uchar   *hebrew(uchar *d, uchar *s)
{
        uchar *p = d ;
        while(*s) {
            if ((*s >='€')&&(*s <='š')) *d = *s - 0x20 ;        // € = 0x80-0x20
            else *d = *s ;
            s++ ; d++ ;
        }
        *d = 0 ; return(p) ;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
void   *memlove(uchar *ptr2, uchar *ptr1, uint t)
{
        while(t--) {
            if (ptr2 < ptr1) *ptr2++ = *ptr1++ ;
            else *(ptr2+t) = *(ptr1+t) ;
            watchdog() ;
        }
        return(ptr2) ;
}
/* * * * * * * * * * * * * * * * * * * * * * * */
void    tools(void)             // dummy function
{
/*
        tools.c
*/
        pause(0) ;
        max(0,0) ;
        min(0,0) ;
        getbitport(0) ;
        message (0,0) ;
        strspace(0,0) ;
        trim(0,0) ;
        fpow(0,0) ;
        hebrew(0,0) ;
        is_space(NULL);
        memlove(NULL, NULL, 0) ;
        trimfix(0) ;
/*
        lcd.c
//
        chargen(0, NULL) ;
        htrans(NULL) ;
*/
}