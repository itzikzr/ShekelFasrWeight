#include "tiger.h"
/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³         High level keyboard and display functions        ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

uchar   keyboard()
{
    uchar k;
    k = 0;

    if (!(porta & 0x10)) return (k);  //jamper w6

    if (kbhit()){
        swkeyb=1;
        k = getch();
        switch (k){
            case 'T': treqt = tensec; break;
            case 'Z': treqz = tensec; break;
            default:;
        }
    }
    return (k);
}

void    testkeyt()
{
    if (!(treqt|treqz)) pass = 0XC0;
    pass += pass + 1;
    treqt = hafsec;
    if (phase > 8 && !ffkey) if (!cdouble('T'))
    {           // T-T keypress (store phase)
        fblink = 1 ;
        phase = 20 ;
        return ;
    }
    ffkey = 0;
    switch (phase)
    {
        case 1:
        case 2:  phase = 3 - phase;
            tblink = 0;
            fblink = 0;
            kdouble(pass);
            break;
        case 6:
        case 7:  factor = rfactor;
            recycle() ; break ;
        case 3:
        case 9:  sformat(icbuf, 0, decim); label("\  DOT ", icbuf);
            ungetch(); display(0); phase = 10; break;
        case 10: wstop = getfvalue("\n STOP ", wstop, decim, 0); if (!returncode)
        {
            fblink = 1 ;
            phase = 20 ;
            break ;
        }
        setdiv();
        phase = false ;
        lcdputs(1,1,"   ROUND        ") ;
        while(!phase)
        {
            wround = getfvalue("\n ROUND", round, decim, 0);
            phase  = rvalid(wround);
        }
        round = wround ;
        if (!returncode)
        {
            fblink = 1 ;
            phase = 20 ;
            break ;
        }
        if (!(com1&0X10)) com1 |= 0X20;
        sprintf(icbuf, "\n%s", ubaudisp[com1&7]);
        phase = (com1>>4)&7; sprintf(icbuf+4, ".%s ", uformat[phase]);
        lcdputs(1,1,"   BAUD1        ") ;
        label("\n BAUD1", icbuf); ungetch(); puts(icbuf); timeout = 2;
        phase = 11;
        break;
        case 11: if (!(com2&0X10)) com2 |= 0X20;
            sprintf(icbuf, "\n%s", xbaudisp[com2&7]);
            phase = (com2>>4)&7; sprintf(icbuf+4, ".%s ", uformat[phase]);
            lcdputs(1,1,"   BAUD2        ") ;
            label("\n BAUD2", icbuf); ungetch(); puts(icbuf); timeout = 2;
            phase = 12;
            break;
        case 12: fblink = 1;
            phase = 13;
            break;
        case 14: fblink = 1; phase = 19; break;
        case 19: fblink = 1; phase = 20; break;
/*
            tblink = fblink = 0 ;
            lcdputs(1,1," SERIAL NUMBER  ") ;
            xbit(ffuse, 1) ;
            sn = getfvalue("   S.N.   ",sn, 0,0) ;
            xbit(ffuse, 0) ;
            if (returncode){ fblink = 1 ; phase = 20 ;}
                if(!returncode){ ungetch()  ; fblink = 1 ; phase = 20 ; }
                break;
*/
            
        case 15: xbit(ftuse, 1) ;
                samprate = (uchar) getfvalue("\nPAR  1", samprate, 0, 0); 
                if (returncode)
                 filmax   = (uchar) getfvalue("\nPAR  2", filmax  , 0, 0); 
                  if (returncode)
                   filrate  = (uchar) getfvalue("\nPAR  3", filrate , 0, 0); 
                    if (returncode)
                     stabin   = (uchar) getfvalue("\nPAR  4", stabin  , 0, 0); 
                      if (returncode)
                       stabout  = (uchar) getfvalue("\nPAR  5", stabout , 0, 0); 
                        if (returncode)
                         stabof   = (uchar) getfvalue("\nPAR  6", stabof  , 0, 0); 
                          if (returncode && oprofi) 
                            profiID  = (uint ) getfvalue("\nPRF ID", profiID , 0, 0); 
                fblink = 1 ;
                phase = 20 ;
            default: break ;
    }
}

void    testkeyz()
{
    if (!(treqt|treqz)) pass = 0XC0;
    pass += pass;
    treqz = hafsec;
    switch (phase)
    {
        case 1:
        case 2:  zref = weight1 = weight0;
            kdouble(pass); break;
        case 6:
        case 7:  factor = rfactor;
            recycle(); break;
        case 15:
        case 9:  fblink = 1;
            break;
        case 10: if ((++decim)>5) decim = 0; display(0); break;
        case 14: //if (bcd)
            //if (++bcdisp>3) bcdisp = 1 ;
            //odeny = ~odeny ;
            if (bcd) if (++bcdisp>3) bcdisp = 1 ;
            else ;
            else odeny = ~odeny ;
        default: break ;
    }
}

void testkeyp()
{
    if (!(treqt|treqz)) pass = 0XC0;
    pass += pass+4;
    treqz = hafsec;
    kdouble(pass);
    switch(phase)
    {
        case 1 :
        case 2 :  puts("\n TRIM ") ;
            if (!cdouble('P'))  phase = 16 ;
            else  phase = 17 ;
            break;
        case 3:   funcon();
            default:  break ;
    }
    treqz = treqt = timeout = 0;
    pass = 0XC0;
}

void    testkeyd(uchar k)
{
    treqz = treqt = timeout = 0;
    pass = 0XC0;
    switch (phase)
    {
        case 9: phase = undisp(6)*10 + k ;
            if (phase<=(sizeof(options)<<3)) k = phase ;
            if ((k)&&(options[(k-1)>>3]&(1<<((k-1)&7))))
                printf("\4-%02u", (uint) k);
            else printf("\4 %02u", (uint) k);
            phase = 9;
            break;
        case 15: phase = undisp(6)*10 + k ;
            if (phase<=(sizeof(hidop)<<3)) k = phase ;
            if ((k)&&(hidop[(k-1)>>3]&(1<<((k-1)&7))))
                printf("\4-%02u", (uint) k);
            else printf("\4 %02u", (uint) k);
            phase = 15;
            break;
        default:;
        }
}

void    display(long n)
{
    switch (decim)
    {
        case 5 : printf("\1%7.5f", ((float) n)*0.00001); break;
        case 4 : printf("\1%7.4f", ((float) n)*0.0001); break;
        case 3 : printf("\1%7.3f", ((float) n)*0.001); break;
        case 2 : printf("\1%7.2f", ((float) n)*0.01); break;
        case 1 : printf("\1%7.1f", ((float) n)*0.1); break;
        default: printf("\1%6ld", n);
    }
    xputs("#");
}

void    sformat(uchar *s, long n, uchar d)
{
    switch (d)
    {
        case 5: sprintf(s, "\1%7.5f", ((float) n)*0.00001); break;
        case 4: sprintf(s, "\1%7.4f", ((float) n)*0.0001); break;
        case 3: sprintf(s, "\1%7.3f", ((float) n)*0.001); break;
        case 2: sprintf(s, "\1%7.2f", ((float) n)*0.01); break;
        case 1: sprintf(s, "\1%7.1f", ((float) n)*0.1); break;
        default: sprintf(s, "\1%6ld", n);
        }
}

void    zformat(uchar *s, uchar d)
{
    switch(d)
    {
        case 0 : sprintf(s, "    -0") ; break ;
        default: sprintf(s, "%7.*f", (uint)d, -0.1/fpow(10,d)) ;
        }
}

void    fformat(uchar *s, float f)
{
    if (f < 0)
    {
        if (f > -10)    sprintf(s, "%7.4f", f) ; else
            if (f > -100)   sprintf(s, "%7.3f", f) ; else
            if (f > -1000)  sprintf(s, "%7.2f", f) ; else
            if (f > -10000) sprintf(s, "%7.1f", f) ; else
            sprintf(s, "%7.0f", f) ;
    }
    else
    {
        if (f < 10)     sprintf(s, "%7.5f", f) ; else
            if (f < 100)    sprintf(s, "%7.4f", f) ; else
            if (f < 1000)   sprintf(s, "%7.3f", f) ; else
            if (f < 10000)  sprintf(s, "%7.2f", f) ; else
            if (f < 100000) sprintf(s, "%7.1f", f) ; else
            sprintf(s, "%7.0f", f) ;
    }
}

uchar   label(uchar *s, uchar *t)
{
    fblink = 1;
    timeout = 0;
    texitfun = exitfun;  
    while (!kbhit())
    {        
        if (!texitfun) if (!strcmp(s," CODE ")){ 
            fexitcode = 1;
            return 'F';
        }
        if (!timeout)
        {
            if (testbit(fblink))
            {
                timeout = hafsec;
                cursor = 1;
                puts(s);
            }
            else
            {
                fblink = 1;
                timeout = twosec;
                cursor = 1;
                puts(t);
            }
        }
        watchdog();
    }
    fblink = 0;
    return (getch());
}
ulong   getvalue(value, maxvalue, start, len)
ulong   value ;                                 // initial value
ulong   maxvalue ;
uint    start ;                                 // start screen position
uint    len ;
{
    ulong lparam ;
    uchar sparam[7], ret ;
    bit   first_z, change_z ;
    bit   first_t, ffirst = true ;

    first_t = first_z = change_z = 0 ;
    ascbuf[0] = start ;
    sprintf(sparam, "%0*ld", len, value) ;
    sprintf(ascbuf+1, "%s", sparam) ;
    _printf(ascbuf) ;

    while(true)
    {
        if (!texitfun && maxvalue == 99) return 0;
        if (!ftest)    blight();
        if (!kbhit())  watchdog() ;
        else
            switch(ret = getch())
        {

            case 'P' : if (xbit(fpuse, test))
            {
                returncode = true ;
                return(-1) ;               // for subtotal print
            }
            first_t = first_z = 0 ;
            break ;

            case 'Z' : first_t = 0 ;
                if (!treqz)
                {
                    first_z = 1 ;
                    treqz = quasec ;
                    while(keypress() == 'Z')
                    {
                        watchdog() ;
                        if (treqz == 0)
                        {
                            treqz  = quasec ;
                            if (testbit(ffirst)) lparam = 0 ;
                            else lparam = atol(sparam) ;
                            if  (lparam == maxvalue) lparam = 1 ;
                            else lparam ++ ;
                            sprintf(sparam, "%0*ld", len, lparam) ;
                            sprintf(ascbuf+1, "%s", sparam) ;
                            _printf(ascbuf) ;
                            change_z = 1 ;
                        }
                    }
                    if (change_z)
                    {
                        change_z = first_z = 0 ;
                    }
                    break ;
                }
                ret = '0' ;

                case '0' : case '1' : case '2' : case '3' : case '4' :
                case '5' : case '6' : case '7' : case '8' : case '9' :
                if (len > 1)
                {
                    if (testbit(ffirst)) sprintf(sparam, "%0*ld", len, 0L) ;
                    memmove(sparam,sparam+1,len-1) ;
                }
                sparam[len-1] = ret ;
                if (atol(sparam) > maxvalue)
                {
                    if  (len > 1)  sprintf(sparam, "%0*ld", len, 0L) ;
                    else ret = '1' ;
                    sparam[len-1] = ret ;
                }
                sprintf(ascbuf+1, "%s", sparam) ;
                _printf(ascbuf) ;
                first_t = first_z = 0 ;
                break ;

            case 'T' : first_z = 0 ;
                if (!treqt)
                {
                    treqt = hafsec ;
                    first_t = 1 ;
                    break ;
                }

                case 'F' : returncode = true ;
                if (xbit(ffuse,2)&(ffirst)) return(0L) ;
                else return(atol(sparam)) ;

                case 'C' : sprintf(sparam, "%0*ld", len, 0L);
                sprintf(ascbuf+1, "%s", sparam) ;
                _printf(ascbuf) ;
                ffirst  = false ;

                default  : first_t = first_z = 0 ;
                break ;
        }
        if ((first_z)&&(!treqz))
        {
            first_z = ffirst = 0 ;
            sprintf(sparam, "%0*ld", len, 0L) ;
            sprintf(ascbuf+1, "%s", sparam) ;
            _printf(ascbuf) ;
        }
        if ((first_t)&&(!treqt))
        {
            returncode = false ;
            if (xbit(ftuse,2))
            {
                if (xbit(ffuse,2)&(ffirst)) return(0L) ;
                else return(atol(sparam)) ;
            }
            else return(value) ;
        }
    }
}
#define tlong   0
#define tfloat  1
#define movedot 0
#define negativ 1
//      get float-point value with flexible point
float   getfvalue(message, value, dec, max)
uchar  *message ;                               // title's message
float   value ;                                 // initial value
uchar   dec ;
ulong   max ;                               // decpoint screen position
{
    long    tvalue ;
    uchar   ret, t ;
    uchar   first_t, ftype ;
    uchar   first_z, change_z ;
    bit     fstart = false ;
    bit     ffirst = true  ;
    int     fminus = true  ;

    first_t = 0 ;
    first_z = change_z = 0 ;
    texitfun = exitfun;
    fendset = 0;
    if (dec == 9)
    {                        // float type
        if (value == 0) dec = 3 ;     else  // decpoint autodetect
            if (value < 10) dec = 5 ;     else
            if (value < 100) dec = 4 ;    else
            if (value < 1000) dec = 3 ;   else
            if (value < 10000) dec = 2 ;  else
            if (value < 100000) dec = 1 ; else dec = 0 ;
        tvalue = value*fpow(10, dec) ;

        ftype  = tfloat ;
    }
    else
    {                                  // long type
        tvalue = value ;
        ftype = tlong ;
    }
    if (tvalue == zeron)
    {
        tvalue =  0 ;
        fminus = -1 ;
        zformat(ascbuf, dec) ;
    }
    else sformat(ascbuf, tvalue, dec) ;
    tblink = 0 ;
    while(true)
    {
        if (!texitfun){ 
            if (!strcmp(message," TARE ")) return value;
            if (!strcmp(message," LOCK ")) return value;
            if (!strcmp(message,"TARE2 ")) return value;
            if (!strcmp(message," NUM  ")) return value;
            if (!strcmp(message," UNIT ")) return kunit;
            if (!strcmp(message,"TARGET")) return value;
            if (!strcmp(message,"TOL-LO")) return value;
            if (!strcmp(message,"TOL-HI")) return value;
            if (!strcmp(message,"GREEN ")) return value;
            if (!memcmp(message,"SET-",4)) {
                fendset = 1;
                return value;
            }
        }
        if(!ftest)   blight();
        if (!kbhit()){
            watchdog() ;
            if (!tblink){
                if (testbit(fblink)){
                    fstart = true ;
                    tblink = twosec ;
                    printf("\n%s", ascbuf) ;
                }
                else{
                    fblink = 1 ;
                    tblink = hafsec ;
                    if (fstart)
                        if ((!undisp(1))&&(fminus<0))
                        zformat(ascbuf, dec) ;
                    else
                        sformat(ascbuf, undisp(1), dec) ;
                    printf("\n%s", message) ;
                }
            }
        }
        else{ 
            switch(ret = getch()){
                case 'P' : if (xbit(fpuse, test)){
                    if (timeout){
                        returncode = 'P' ;
                        tblink = 0 ;
                        return(-1) ;
                    }
                    timeout = hafsec ;
                }
                first_t = first_z = 0 ;
                break ;
    
                case 'Z' : first_t = 0 ;
                    if (!fstart) break ;
                if (!treqz){
                    first_z = 1 ;
                    treqz = quasec ;
                    while(keypress() == 'Z'){
                        watchdog() ;
                        if (treqz == 0){
                            treqz  = quasec ;
                            if (testbit(ffirst)){
                                sformat(ascbuf, 0, dec) ; printf("\n%s", ascbuf) ;
                            }
                            t = unput(6)&0x7f;
                            if (++t > '9') t = '0';
                            printf("\6%c", t) ;
                            tvalue = undisp(1) ;
                            change_z = 1 ;
                        }
                    }
                    if (change_z){
                        change_z = first_z = 0 ;
                    }
                    tblink = fullsec ;
                    break ;
                }
                ret = '0' ;
                if (testbit(ffirst)) ;
    
                case '0' : case '1' : case '2' : case '3' : case '4' :
                case '5' : case '6' : case '7' : case '8' : case '9' :
                        if (!fstart) break ;
                        if (testbit(ffirst)){
                            sformat(ascbuf, 0, dec) ; printf("\n%s", ascbuf) ;
                        }
                        tvalue = undisp(2)*10 ;
                        if (!memcmp(message,"SET-",4) && oricun) fminus = -1 ; 
                        else{
                            if (tvalue < 0)  fminus = -1 ; else
                                if (tvalue > 0)  fminus =  1 ;
                        }
                        tvalue += (ret-'0') * fminus ;
                        if (max && tvalue > max) tvalue = (ret-'0') * fminus ;
                        sformat(ascbuf, tvalue, dec);
                        printf("\n%s", ascbuf) ;
                        first_t = first_z = 0 ;
                        tblink  = fullsec ;
                        break ;
        
                case 'T' : first_z = 0 ;
                        if (!treqt){
                            treqt = hafsec ;
                            first_t = 1 ;
                            break ;
                        }
    
                case 'F' : if (!fstart) break ;
                        tblink = 0 ;
                        returncode = true ;
                        if (ftype == tlong)
                        {
                            if ((!tvalue)&&(fminus<0)) byte(tvalue, 0) = 0x80 ;
                            return tvalue ;
                        }
                        return (float)tvalue/fpow(10, dec) ;
        
                case 'C' : sformat(ascbuf, 0, dec) ; printf("\n%s", ascbuf) ;
                        fminus  = true  ;
                        tvalue  = false ;
                        ffirst  = false ;
                        tblink  = fullsec ;
        
                        default  : first_t = first_z = 0 ;
                        break ;
            }
            texitfun = exitfun;
        }
        if ((first_z)&&(!treqz)){
            first_z = first_t = 0 ;
            if (xbit(fzenb, test)) switch((int)xbit(fzuse, test))
            {
                case movedot: if (!dec--) dec = 5 ;
                    sformat(ascbuf, tvalue, dec) ;
                    printf("\n%s", ascbuf) ;
                    if (xbit(fdoff, test)) decperiod = dec ;
                    break ;
                case negativ: fminus *= -1 ;
                    tvalue *= -1 ;
                    ffirst  = false ;
                    tblink  = fullsec ;
                    if ((!tvalue)&&(fminus<0))
                        zformat(ascbuf, dec) ;
                    else
                        sformat(ascbuf, tvalue, dec) ;
                    printf("\n%s", ascbuf) ;
                }
            else
            {
                sformat(ascbuf, 0, dec) ; printf ("\n%s", ascbuf) ;
                fminus  = true  ;
                tvalue  = false ;
                ffirst  = false ;
                tblink  = fullsec ;
            }
        }
        if ((first_t)&&(!treqt))
        {
            tblink = 0 ;
            returncode = false ;
            if (xbit(ftuse, test))
                if (ftype == tlong)  return(tvalue) ;
            else  return((float)tvalue/fpow(10, dec)) ;
            else  return(value) ;
        }
    }
}

ulong   undisp(uchar k) small
{
    uchar i;
    i = 0;
    if (k)
    {
        while (k<7) icbuf[i++] = unput(k++)&0X7F;
        icbuf[i] = 0;
        return (atol(icbuf));
    }
    else return (0);
}

uchar   unput(uchar c) small
{
    uchar   i;
    c = disflag[c-1];
    for (i=0;i<26;i++)
    {
        if (magtab[i]==(c&0XDF)) break;
    }
    if (i<26)
    {
        i += ' ';
        if (c&0X20) i |= 0X80;
    }
    else i = ' ';
    return (i);
}

bit     rvalid(uint r)
{
    uint rvalue[] = { 1,2,5,10,20,25,50,100,200,250,500 } ;
    uint t = 0, m = sizeof(rvalue)/2 ;

    while(t < m) if (r == rvalue[t++]) return(true) ;
    return(false) ;
}
