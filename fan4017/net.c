#include "fanbase.h"

/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³                      Net Protocol                        ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ

*/

uchar   cs1(uchar *s);

#define byte2(n, m) * (((uchar *) (&n)) + m)
#define is_checksum(x)  ((netcs|0x40) == x)

void     network(uchar ret)
{
    switch(phext){
        case 0 : if (ret == '#'){             // start of request string
                     phext = 1 ;
                     netcs = 0  ;
                 }
                 break ;
        case 1 : phext = 2 ;
                 netstring[0] = ret ;      // byte0 of net scale number
                 break ;
        case 2 : phext = 3 ;
                 netstring[1] = ret ;      // byte1 of net scale number
                 netstring[2] = 0 ;
                 if (net - atoi(netstring)) phext = 0 ;
                 break ;
        case 3 : switch(ret){                     // request properly
                     case lf : phext = 0 ;
                               answer(ack1) ;   // polling
                               break ;
                     case 0x06:                // acknowledge
                     case 0x15:                        // noacknowledge
                     case netweight:                       // weight & keys of relsys
								  	 case nettareval:                      // Get Trae Value
                     case nettare:                         // tare function
                     case netzero:                         // zero function
                          phext = 4 ;
                          request = ret ;
                          break ;
                     default : phext = 0 ;
                          answer(nak1) ;
                 }
                 break;
        case 4 : phext = 0 ;                       // checksum for 'w-t' request
                 if (!is_checksum(ret)) answer(nak1) ;
                 else{
                     if (errnum) request = netweight;
                     if (request == 0x06){
                         if (await) await = 0 ;
                     }
                     if (request == 0x15){
                         if (await) request = savuest ;
                     }
                     if (request == netweight){
                         phext = 5 ;
                         wttrans() ;
                     }
										 
										 if (request == nettareval){
                         phext = 5 ;
                         ttrans() ;
                     }
										 
                     if (request == nettare){
                         treqt = twosec ; 
                         ftranstare = 1;
                     }
                     if (request == netzero){
                         treqz = twosec ;
                         ftranzero = 1;
                     }
                     if (phext == 5){
                         await = 1 ;
                         savuest = request ;
                     }
                 }
                 break ;
        case 5 : if (ret == '#'){ // start of acknowledge #<NN><ACK><CS><LF>
                     phext = 1 ;
                     netcs = 0  ;
                 }
                 break ;
    }
    if (phext){
        netcs ^= ret ;
        trsnet = 10;//fulsec ;           // 3.33ms * 255 = 0.85 sec
    }
}

void    answer(uchar *b)
{
	if (onet){
	    sprintf(netbuf, "#%02d%s", (uint)net,b) ;
	    netbuf[4]=0;
	    netbuf[4] = ncs(netbuf) ;
	    netbuf[5] = lf  ;
	    binstring(netbuf, 6) ;
	}else{
	    netbuf[0] = b[0]  ;
	    netbuf[1] = lf  ;
	    binstring(netbuf, 2) ;
	}
    ftranstare = ftranzero = 0;
}

uchar   ncs(uchar *s)
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
    for(t=0;t<=11;t++)ret ^=s[t];
    return ret|0x40 ;           // 0x40 - to avoid CS = LF
}

double  fabs(double) ;
void    wttrans(void)
{
    uchar   d ;

    if ((funder && oneg) || fover || errnum){
        if (funder) strcpy (ascbuf, "  UNDER  ") ;
        if (fover ) strcpy (ascbuf, "   STOP  ") ;
        if (errnum) sprintf(ascbuf, "ERROR-%02d ",(uint)errnum);
    }else{
        //d = (funder || fngz) ? '-' :  '+';
				if (funder || fngz || hibyte(brut)) d = '-';
				else                                d = '+';
        sprintf(ascbuf, "%c%c%06ld", (fstab)? 'S': 'U', d, labs(brut)) ;
        if (DecimalPointNum(disform)){
            d = 8 - DecimalPointNum(disform) ;
            memmove(ascbuf+d+1, ascbuf+d, 10-d) ;
            ascbuf[d] = '.' ;
        }
        else
            sprintf(ascbuf, "%c%c%07ld", (fstab)? 'S': 'U', d, labs(brut)) ;
    }
    sprintf(netbuf, "#%02d%s", (uint)net, ascbuf) ;
    netbuf[12] =(uchar)cs1(netbuf) ;
    netbuf[13] = lf ;
    binstring(netbuf, 14) ;
    fnew = 0;
}

void    ttrans(void)
{
		uchar d;
	
		if (funder || fngz || hibyte(brut)) d = '-';
		else                                d = '+';
	
		sprintf(ascbuf, "T%c%06ld", d, wtare(tare)) ;
    if (DecimalPointNum(disform)){
        d = 8 - DecimalPointNum(disform) ;
        memmove(ascbuf+d+1, ascbuf+d, 10-d) ;
        ascbuf[d] = '.' ;
    }
    else
        sprintf(ascbuf, "T%c%07ld", d, wtare(tare)) ;

    sprintf(netbuf, "#%02d%s", (uint)net, ascbuf) ;
    netbuf[12] =(uchar)cs1(netbuf) ;
    netbuf[13] = lf ;
    binstring(netbuf, 14) ;
    fnew = 0;
}