#include "tiger.h"
#include "title.h"
/*
⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
≥                      Print routines                      ≥€
¿ƒ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹€

*/

void    totalizing(void)
{
    order  += 1 ;
    gorder += 1 ;
    if (fcount)  dnetto = cnetto() ;
    else  dnetto = (float)wdisp/fpow(10, decim) ;
    total += dnetto ;
    gtotal += dnetto ;
    wprint = wdisp  ;
}

float   cnetto(void)
{
    long    lw ;
    float   fw ;

    if (fstab) lw = weight1 - zref;
    else lw = weight0 - zref;
    fw = lw - tare;
    fw *= wfactor ;
    if (byte(fw,3)&0X80) fw -= 0.5;
    else fw += 0.5;
    lw  = (long) fw;
    lw *= wround;
    return((float)lw/fpow(10,wdecim)) ;
}

#define is_checksum(x)  ((netcs|0x40) == x)
bit     network(uchar ret)
{
//sprintf(secbuf,"\n-%c",ret); comstring(secbuf); while(fsend);
    switch(phext)
    {
        case 0 :  if (ret == '#')
        {             // start of request string
            phext = 1 ;
            netcs = 0  ;
        }
        break ;
        case 1 :  phext = 2 ;
            netstring[0] = ret ;      // byte0 of net scale number
            break ;
        case 2 :  phext = 3 ;
            netstring[1] = ret ;      // byte1 of net scale number
            netstring[2] = 0 ;
            if (net - atoi(netstring)) phext = 0 ;
            break ;
        case 3 :  switch(ret)
        {                     // request properly
            case lf : phext = 0 ;
                answer(ack) ;   // polling
                break ;
            case 0x06:                // acknowledge
            case 0x15:                        // noacknowledge
            case netweight:                       // weight & keys of relsys
            case getrelay :           // relays status
            case nettare:                         // tare function
            case netzero:                         // zero function
                phext = 4 ;
                request = ret ;
                break ;
            case netsetpoint:                     // setpoints setting
            case netcweigher:
                phext = 6 ;
                counter = 0 ;
                request = ret ;
                break ;
            case setrelay:            // set relays
                phext = 7 ;
                break ;
            default : phext = 0 ;
                answer(nak) ;
            }
        break ;
        case 4 :  phext = 0 ;                       // checksum for 'w-t' request
            if (!is_checksum(ret)) answer(nak) ;
            else
            {
                if (request == 0x06)
                {
                    if (await) await = 0 ;
                }
                if (request == 0x15)
                {
                    if (await) request = savuest ;
                }
                if (request == netweight)
                {
                    phext = 5 ;
                    wttrans() ;
                }
                if (request == getrelay)
                {
                    phext = 5 ;
                    wttrans() ;
                }
                if (request == nettare)
                {
                    treqt = twosec ;
                    transtare  = 1 ;
                    ftranstare = 1;
                }
                if (request == netzero)
                {
                    treqz = twosec ;
                    transzero = 1 ;
                    ftranzero = 1;
                }
                if (phext == 5)
                {
                    await = 1 ;
                    savuest = request ;
                }
            }
            break ;
        case 5 :  if (ret == '#')
        {             // start of acknowledge #<NN><ACK><CS><LF>
            phext = 1 ;
            netcs = 0  ;
        }
        break ;
        case 6 :  switch(ret)
        {
            case lf : ret = netbuf[counter-1] ;   // previous byte - checksum
//flag(fmidl,flag(fmidl,test)^1); comstring(netbuf); while(fsend); break;
                netcs ^= ret ;      // disX-or checksum from netcs
                if (!is_checksum(ret)) answer(nak) ;
                else
                {
                    netbuf[counter-1] = 0 ;
                    if (setrans(request)) answer(ack) ;
                    else                  answer(nak) ;
                }
                phext = 0 ;
                break ;
            default : if (counter < 160) netbuf[counter++] = ret ;
                else
                {
                    phext = 0 ;
                    answer(nak) ;
                }
                break ;
        }
        break ;
        case 7 :  phext = 8 ;
            netbuf[0] = ret ;
            break ; // relay command
        case 8 :  phext = 9 ;
            netbuf[1] = ret ;
            break ;
        case 9 :
            if (!is_checksum(ret))  answer(nak) ;
            else
            {
                byte(imrel, 0) = netbuf[0] ;
                byte(imrel, 1) = netbuf[1] ;
                wttrans() ;
                /*
                                              answer(ack) ;
                                              */
            }
            phext = 0 ;
            break ;
    }
    if (phext)
    {
        netcs ^= ret ;
        trsnet = fullsec ;           // 3.33ms * 255 = 0.85 sec
    }
}

void    answer(uchar *b)
{
    sprintf(netbuf, "#%02d%s", (uint)net,b) ;
    netbuf[4]=0;
    netbuf[4] = cs(netbuf) ;
    netbuf[5] = lf  ;
    if (!comport) binstring(netbuf, 6) ;
    else         xbinstring(netbuf, 6) ;

}
uchar   cs(uchar *s)
{
    uint    t ;
    uchar   ret ;

    ret = t = 0 ;
    while(s[t])ret ^= (uchar)s[t++] ;
    return ret|0x40 ;           // 0x40 - to avoid CS = LF
}
uchar   cs1(uchar *s)
{
    uint    t ;
    uchar   ret ;

    ret = t = 0 ;
    //while(s[t])ret ^= (uchar)s[t++] ;
    for(t=0;t<=16;t++)ret ^=s[t];
    return ret|0x40 ;           // 0x40 - to avoid CS = LF
}
bit setrans(uchar req)
{
    uchar   n, num, ret ;
    uchar   relay = 0 ;
    uchar   s[15] , t ;
    long    maset[spnum] ;
    uchar   m = spnum ;

    if (netbuf[0] - req) return false ;
    while(m) maset[--m] = 0 ;

    num = n = 1 ;
    do
    {
        ret = netbuf[n++] ;
        if ((!ret)||(ret == req)) num = 5 ;
        switch(num)
        {
            case 1 : t = 0 ;
                num = 2 ;
                case 2 : s[t++] = ret ;
                if (t > 1)
                {
                    s[t] = 0 ;
                    relay = atoi(s) ;
                    if (!relay||(relay>spnum)) return false ;
                    num  = 3 ;
                }
                break ;
            case 3 : t = 0 ;
                num = 4 ;
                case 4 : s[t++] = ret ;
                if (t < 14) break ;
            return false ;
            case 5 : if (relay)
            {
                s[t] = 0 ;
                maset[m] = atof(s)*fpow(10, decim) ;
                byte(maset[m], 0) = relay ;
                m += 1 ;
            }
            num = 1 ;
            break ;
        }
        watchdog() ;
    }
    while (ret) ;
    switch(req)
    {
        case netsetpoint :  n = m = 0 ;
            while(n < spnum)
            {
                ret = byte(maset[n], 0) ;
                switch(ret)
                {
                    case  7 :
                    case  8 : break ;
                default : spdata[m++] = maset[n] ;
                    }
                n++ ;

            }
            spmax = m ;
            break ;
        case netcweigher :  n = 0 ;
            while(n < m)
            {
                ret = byte(maset[n], 0) ;
                byte(maset[n], 0) = 0 ;
                if (ret == 1) tollo  = maset[n] ; else
                    if (ret == 2) target = maset[n] ; else
                    if (ret == 3) tolhi  = maset[n] ; else
//                    if (ret == 4) greendiv = maset[n] ; else
                    return false ;
                n++ ;
            }
        }
    return true ;
}

double  fabs(double) ;
void    wttrans(void)
{
    uchar   d ;
//    float   fdisp ;

    if (onet)
    {
        switch((uint)fover)
        {                           // under/overload weight flag
            case 0:
                if (wdisp < 0)  d = '-' ; else
                    if (opsign)     d = ' ' ; else
                    d = '+' ;
                sprintf(ascbuf, "%c%c%06ld", (fstab)? 'S': 'U', d, labs(wdisp)) ;
                if (decim)
                {
                    d = 8 - decim ;
                    memmove(ascbuf+d+1, ascbuf+d, 10-d) ;
                    ascbuf[d] = '.' ;
                }
                else
                    sprintf(ascbuf, "%c%c%07ld", (fstab)? 'S': 'U', d, labs(wdisp)) ;
                break ;

            case 1:
                if (fsign) strcpy(ascbuf, "  UNDER  ") ;
                else strcpy(ascbuf, "   STOP  ") ;
            }
        sprintf(netbuf, "#%02d%sK%cR%c%c", (uint)net, ascbuf, keystatus,
                byte(imrel, 0), byte(imrel, 1)) ;
        netbuf[17] =(uchar)cs1(netbuf) ;
        netbuf[18] = lf ;
        if (!comport) binstring(netbuf, 19) ;
        else         xbinstring(netbuf, 19) ;
    }
    else
    {
        fdisp = (float)wdisp/fpow(10, decim) ;
        switch((uint)fover)
        {                           // under/overload weight flag
            case 0:
                if (ospace)
                {
                    if (opsign) sprintf(ascbuf, "%8.*f\r", (uint)decim, fdisp) ;
                    else sprintf(ascbuf, "%+8.*f\r", (uint)decim, fdisp) ;
                }
                else
                {
                    if(oastrk)
                    {
                        if (fdisp < 0)  d = '-' ; else
                            if (opsign)     d = ' ' ; else
                            d = '+' ;
                        sprintf(ascbuf, "*%c%07.*f\r", d, (uint)decim, fabs(fdisp)) ;
                    }
                    else
                    {
                        if (fdisp < 0)  d = '-' ; else
                            if (opsign)     d = ' ' ; else
                            d = '+' ;
                        sprintf(ascbuf, "%c%07.*f\r", d, (uint)decim, fabs(fdisp)) ;
                    }
                }
                break ;
            case 1:
                if (fsign) strcpy(ascbuf, " UNDER  \r") ;
                else strcpy(ascbuf, "  STOP  \r") ;
            }
        if (!comport) comstring(ascbuf) ;        // 0 - internal UART
        else         xcomstring(ascbuf) ;        // 1 - external UART
    }
}

void    ttrans(void)
{
    float   dtare ;

    dtare = (float)factorize(tare)/fpow(10, decim) ;
    sprintf(ascbuf, "%8.*f\r", (uint)decim, dtare) ;
    
    if (!comport) comstring(ascbuf) ;        // 0 - internal UART
    else         xcomstring(ascbuf) ;        // 1 - external UART
}

void    TotalTrans(uchar t)
{
    if (t == 'S') sprintf(ascbuf, "T%07.*f\r", (uint)decim, total) ;
    if (t == 'N') sprintf(ascbuf, "N%07d\r", order) ;
    if (t == 'C') {
        total = order = 0;
        ascbuf[0]= 'C';
        ascbuf[1]= 0x0d;
        ascbuf[2]= 0;
//        sprintf(ascbuf, "C\r") ;
    }
    
    comstring(ascbuf) ;        // 0 - internal UART
}

void sendweight()
{
    uchar s=0x08, sum, c1, c2;
    uint  w;
//    float fdisp ;

    fdisp = (float)wdisp/fpow(10, decim) ;
    if (fdisp < 0) s |= 0x06;
    if (fover){          // under/overload weight flag
        if (fsign) s |= 0x02; // under
        else       s |= 0x04; // overload
    }
    if (!fstab ) s |= 0x01;
    if (fstatus && !oldprofi) s |= 0x80;

    if (wdisp >= 65500){
        w = 65500;
        s |= 0x04;
    }
    else w = (uint)wdisp;

    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = s + c1 + c2;
    sprintf(ascbuf,"%c%c%c%c",s, c1, c2, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}

void sendidentifier()
{
    uchar sum, c1, c2;
    uint  w;

    fstatus = 0;
    if (!odfrofi && !oldprofi) w = (uint)ident;
    else                       w = 0;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'I' + c1 + c2;
    sprintf(ascbuf,"I%c%c%c", c1, c2, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}

void sendatod()
{
    uchar sum, c1, c2;

    c1 = byte(atod , 0);
    c2 = byte(atod , 1);
    sum = 'R' + c1 + c2;
    sprintf(ascbuf,"R%c%c%c", c1, c2, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}

void sendfullscale()
{
    uchar sum, c1, c2;
    uint  w;

    w = (uint)wfull;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'F' + c1 + c2;
    sprintf(ascbuf,"F%c%c%c", c1, c2, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}

void sendtarevale()
{
    uchar sum, c1, c2;
    uint  w;

    w = factorize(tare);
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'V' + c1 + c2;
    sprintf(ascbuf,"V%c%c%c", c1, c2, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}

void resersacle()
{
    uchar sum;

    sum = 'S';
    sprintf(ascbuf,"S%c%c%c", 0, 0, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
    while(1);
}

void sendzero()
{
    uchar sum;

    sum = 'Z';
    sprintf(ascbuf,"Z%c%c%c", 0, 0, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}

void sendtare()
{
    uchar sum;

    sum = 'T' ;
    sprintf(ascbuf,"T%c%c%c", 0, 0, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}

void senddecimalpoint()
{
    uchar sum, c1, c2;
    uint  w;

    w = (uint)decim;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'D' + c1 + c2;
    sprintf(ascbuf,"D%c%c%c", c1, c2, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}
/*
void sendaddress()
{
    uchar sum, c1, c2;
    uint  w;

    w = (uint)profiadd;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'A' + c1 + c2;
    sprintf(ascbuf,"A%c%c%c", c1, c2, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}

void sendprofiID()
{
    uchar sum, c1, c2;
    uint  w;

    w = (uint)profiID;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'J' + c1 + c2;
    sprintf(ascbuf,"J%c%c%c", c1, c2, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}
*/
void senderror(uchar n)
{
    uchar sum;

    sum = 'E' + n;
    sprintf(ascbuf,"E%c%c%c", 0, n, sum);
    if (!comport) binstring(ascbuf, 4) ;        // 0 - internal UART
    else         xbinstring(ascbuf, 4) ;        // 1 - external UART
}
