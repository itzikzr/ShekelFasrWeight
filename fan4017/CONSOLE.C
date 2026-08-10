#include "fanbase.h"

void display(long n, uchar k)
{
    xdata long m;
    //if (!testbit(fdisp)) return;
    k &= 0X87;
    if (hibyte(n)){
        k |= 0X40;
        n = -n;
    }
    if (fngz) k |= 0x40;
    sprintf(disbuf, "%06lu", n);
    hibyte(n) = k & 7;
    if (hibyte(n)){
        if (k & 0X80) hibyte(n)--;
    }

    else hibyte(n) = 5;
    cursor = 0;
    while (cursor<hibyte(n)){
        if (disbuf[cursor]=='0') disbuf[cursor] = ' ';
        else break;
        cursor++;
    }
    if (k & 0X40)
    {
        m = n;
        hibyte(m) = 0;
        if (labs(m) > 99999){
            if (!tdsneg){
                tdsneg = 10 ;
                fdsneg ^= 1 ;
            }
        }else fdsneg = 0;
        if (!fdsneg){
            if (cursor) cursor--;
            disbuf[cursor] = '-';
        }
    }
    cursor = 1;
    hibyte(n) = 0;
    while (hibyte(n)<6)
    {
        putchar(disbuf[hibyte(n)]);
        if ((++hibyte(n))==(k&7)) putchar('.');
    }
    if (opbigdsp) xputs();
}

void altern(uchar *s, ulong n, uchar mode)
{
    if (!talter){
        falter = !falter;
        if (falter){
            cursor = 1;
            puts(s);
            talter = hafsec;
        }else talter = onesec;
    }
    if (!falter){
        fdisp = 1;
        display(n, mode);
    }
}

ulong getdata(uchar *s, ulong n, uchar f, ulong m)
{
    xdata uchar d;
    xdata ulong b;

    b = n;
    fngz = 0;
    if (m > 999999L) m = 0;
    if (!m) m = 999999L;
    if (n > m) n = m;
    talter = falter = 0;
    while (!fkey){
//         if (strcmp(s,"4 - 20") && strcmp(s," BCD  ")) bcdbuf = bcd(0 , 2);
        altern(s, n, f);
        watchdog();
    }
    if (kzero){ 
        n = 0;
        fdisp = 1;
        display(n, f);
    }
    if (testbit(falter)){
        fdisp = 1;
        display(n, f);
    }
    timout = 0;
    d = n % 10;
    n -= d;
    do{
//        if (strcmp(s,"4 - 20") && strcmp(s," BCD  ")) bcdbuf = bcd(0 , 2);
        if (testbit(fkey)){
            talter = hafsec;
            if (timout){
                if (kzero){
                    n += d;
                    n *= 10;
                    d = 0;
                    if (n>m) n = 0;
                    fdisp = 1;
                    display(n, f);
                }
                timout = 0;
            }
            else timout = hafsec;
        }
        if (kzero){
            if (!talter){
                talter = hafsec;
                if ((++d)>9) d = 0;
                if ((n+d)>m) d = 0;
                fdisp = 1;

                if (!strcmp(s, "\n ROUND")){ // round only 1 2 5 10 20 50 100 200
                    if (!d) d = 1;
                    if (d > 5) d = 1;
                    if (d > 2 && d != 5) d = 5;
                    if (n) d = 0;
//sprintf(ascbuf,"\n%ld %d",n,(uint)d); comstring(ascbuf); while(fsend);
                }

                display(n + d, f);
            }
        }
        else talter = hafsec;
        watchdog();
    }
    while (!ktare);
    fkey = 0;
    n += d;
    return (n);
}

bit  cdouble(uchar c)
{
    timeout = 4 ;
    while(timeout){
        if (kbhit()){
            if(getch(0) == c){
                timeout = 0 ;
                return 1 ;
            }
        }
        watchdog() ;
    }
    return 0 ;
}

void pause(uchar t)
{
    timeout = t;
    while(timeout){
        bcdbuf = bcd(0 , 2);
        watchdog();
    }
}

uint getvalue(uchar *s , uint d, uchar n, ulong max)
{
    xdata uchar ret, i, j, f=0;
    
    printf("\n%s%02d",s,d);
    j = 0;
    i = d;
    do{
        bcdbuf = bcd(0 , 2);
        if (kbhit()){
            ret = getch(0);
            switch(ret){
                case 'Z':   f = 1;
                            if(cdouble('Z')){
                                j = i;
                                i = 0;
                                d = (uint)i + (uint)j*10;
                                if (d > max) j = i = 0; 
                                printf("\n%s%d%d",s,(uint)j,(uint)i);
                            }
                            else{
                                timeout = 0;
                                do{
                                    watchdog();
                                    if (!timeout){
                                        timeout = 4;
                                        i += 1;
                                        if (i > 9){
                                            j +=1;
                                            i = 0;
                                        }
                                        if (j > 9) j = 0;
                                        d = (uint)i + (uint)j*10;
                                        if (d > max) j = i = 0; 
                                        printf("\n%s%d%d",s,(uint)j,(uint)i);
                                    }
                                }
                                while(kzero);
                            }
                            break;
                case 'T':   
                            if(cdouble('T')){
                                d = (uint)i + (uint)j*10;
                                twait = 10;
                                if (!f) d = 0;
                                return d;
                            }
                            else{
                                if (n) return 0;
                                else{
                                    if (f){
                                        d = (uint)i + (uint)j*10;
                                        return d;
                                    }
                                    else return 0;
                                }
                            }
                case 'P':   if (n) return 0;
                            else{
                                if (f){
                                    d = (uint)i + (uint)j*10;
                                    return d;
                                }
                                else return 0;
                            }
            }
        }
        watchdog();
    }
    while(1);
}

void blink(uchar *s, uchar *l , uchar t)
{
    xdata uchar fblink;

    tblink = fblink = 0 ;
    clrscr();
    dmr = dml = 1;
    while(!kbhit()){
        watchdog() ;
        bcdbuf = bcd(0 , 2);
        if (!tblink){
            tblink = t ;
            fblink ^= 1 ;
            if (fblink) printf("\n%s", s) ;
            else        printf("\n%s", l) ;
        }
    }
}

ulong   ConvertLongByte(uchar *s)
{
    xdata ulong n;

    inbyte(n, 0) = s[3];
    inbyte(n, 1) = s[2];
    inbyte(n, 2) = s[1];
    inbyte(n, 3) = s[0];
    return (n);
}

bit  xflag(ulong x, uchar n)
{
    xdata ulong l;

    l = 0x80000000;
    l >>= n;
    if (l & x) return 1;
    else       return 0; 
}

void    discon(uchar *s)
{
    if (s==NULL) flcd = 1;
    else {
        clrscr();
        puts(s);
        frame = 0;
        while (!frame) watchdog();
        frame = 0;
        while (!frame) watchdog();
        flcd = 0;
    }
}
