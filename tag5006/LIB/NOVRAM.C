
#include "tigerl.inc"
#include "tigerl.h"

/*
                    ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
                    ³                       ³Û
                    ³     NOVRAM handlers   ³Û
                    ³                       ³Û
                    ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

bit     novin(uchar *m, uchar p) small
{
        p &= 0XF;
        p <<= 3;
        p = open(novad, p);
        if (p) p = read(novad, isize);
        close();
        if ((p)&&(m!=NULL)) memcpy(m, icbuf, isize);
        return (p);
}

bit     novout(uchar p, uchar *m) small
{
        if (m!=NULL) memcpy(icbuf, m, isize);
        p &= 0XF;
        p <<= 3;
        p = open(novad, p);
        if (p) p = write(isize);
        close();
        return (p);
}
