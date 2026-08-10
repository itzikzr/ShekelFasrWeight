
#include "tigerl.inc"
#include "tigerl.h"

/*
                    ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
                    ³                       ³Û
                    ³      RTC handlers     ³Û
                    ³                       ³Û
                    ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

bit     clockin(uchar *m, uchar p) small
{
        p &= 7;
        p <<= 3;
        p = open(rtcad, p);
        if (p) p = read(rtcad, isize);
        close();
        if ((p)&&(m!=NULL)) memcpy(m, icbuf, isize);
        return (p);
}

bit     clockout(uchar p, uchar *m) small
{
        if (m!=NULL) memcpy(icbuf, m, isize);
        p &= 7;
        p <<= 3;
        p = open(rtcad, p);
        if (p) p = write(isize);
        close();
        return (p);
}
