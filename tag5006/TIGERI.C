#include "tiger.h"
/*
⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
≥                  User keyboard interface                 ≥€
¿ƒ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹€

0  - toggle0()   - toggle between counting and weighing mode
1  - toggle1()   - toggle auto/manual totalizing (printing)
2  - getcode()   - enter the actual code number
3  - define_cw() /
define_sp() - define the checkweigher/setpoints parameters
4  - subdisp()   - current subtotal and order display
5  - csample()   - set counting mode by sample
6  - unitwgt()   - set counting mode by unit weight
7  - membyte()   - external memory size in Kbytes
8  - brutwgt()   - display of current brute weight
9  - clear_tot(0)- clear of all subtotals

F1  - datime()    - set date and time values
F2  - ranges()    - set the zero ranges for relayZ(ero) and start-tare
F3  - ranset()    - set the low/high ranges for auto totalizing
F4  - getnam()    - get product's name
F7  - delete()    - erase a code from memory
F8  - cdelay()    - delay in the self-latching point
F9  - smcode()    - mcode area status (free/busy by codes)
F10 - cdelay()    - delay in the zero point after self-latching
F11 - splong()    - setpoint's long time definition
F12 - _dload()    - dead weight zone for setpoints
F13 - cdelay()    - checkweigher's delay to the relcon
F14 - manual_sp() - manual test of setpoints
F98 - clear_tot(1)- clear of all sub and grandtotals
F99 - clear_all() - clear of all code
*/
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
0 - Toggle between counting and weighing
*/
void    toggle0(void)
{
    uint  t,i;
    ulong set;

    if (ocount){
        if ((kunit) && (fcount ^= 1)){               // counting mode
            rfactor = factor / kunit * fpow(10, 3 - (int)wdecim) ;
            wstop   = wwstop / kunit * fpow(10, 3 - (int)wdecim) ;
            round = 1 ;
            decim = 0 ;
            if ((fwc == 2 || ocheck) && kunit)
            {
                if (ocheck){
                    spdata[0] = target  ;
                    spdata[1] = tollo   ;
                    spdata[2] = tolhi   ;
                    spdata[3] = 0;//greendiv;
                }
                if (ocheck){
                    target   = spdata[0] ;
                    tollo    = spdata[1] ;
                    tolhi    = spdata[2] ;
                    //greendiv = spdata[3] ;
                }
            }
            if (!fcountstart){
                ranlo /= kunit;
                ranhi /= kunit;
            }
            if (fcounttoggle)
            {
                kunit1 = kunit1 / kunit;
                for (t = 0 ; t < 15 ; t++){
                    set = spdata[t] ;
                    i = byte(set,0) - 1 ;
                    byte(set,0) = (byte(set,1)&0x80)? 0xff: 0 ;
                    spdata[t] = set * kunit1;
//sprintf(ascbuf,"\n1  %ld  %f ",set , kunit1); comstring(ascbuf); while(fsend) watchdog();
                    byte(spdata[t], 0) = i + 1; 
                    watchdog();
                }
                if (ocheck){
                    target   = spdata[0] ;
                    tollo    = spdata[1] ;
                    tolhi    = spdata[2] ;
//                    greendiv = spdata[3] ;
                }                           
            }
            else{
                if (!fcountstart){
                    for (t = 0 ; t < 15 ; t++){
                        set = spdata[t] ;
                        i = byte(set,0) - 1 ;
                        byte(set,0) = (byte(set,1)&0x80)? 0xff: 0 ;
                        spdata[t] = set / kunit;
//sprintf(ascbuf,"\n2  %ld  %f  %f",set , kunit, (set / kunit)); comstring(ascbuf); while(fsend) watchdog();
                        byte(spdata[t], 0) = i + 1; 
                        watchdog();
                    }
                }
                if (ocheck){
                    target   = spdata[0] ;
                    tollo    = spdata[1] ;
                    tolhi    = spdata[2] ;
//                    greendiv = spdata[3] ;
                }
            }   
            fcountstart = 0;
        }
        else{   
            if (ocheck) light(none) ;
            fcount  = 0      ;
            rfactor = wfactor;
            round   = wround ;
            wstop   = wwstop ;
            decim   = wdecim ;
            if ((fwc == 1  || ocheck) && kunit)
            {
                if (ocheck){
                    spdata[0] = target  ;
                    spdata[1] = tollo   ;
                    spdata[2] = tolhi   ;
                    spdata[3] = 0;//greendiv;
                }
                if (ocheck){
                    target   = spdata[0] ;
                    tollo    = spdata[1] ;
                    tolhi    = spdata[2] ;
//                    greendiv = spdata[3] ;
                }
            }
            if (kunit){
                ranlo *= kunit;
                ranhi *= kunit;
                for (t = 0 ; t < 15 ; t++){
                    set = spdata[t] ;
                    i = byte(set,0) - 1 ;
                    byte(set,0) = (byte(set,1)&0x80)? 0xff: 0 ;
                    spdata[t] = set * kunit;
                    byte(spdata[t], 0) = i + 1;
//sprintf(ascbuf,"\n3  %ld  %f  %f",set , kunit, (set / kunit)); comstring(ascbuf); while(fsend) watchdog();
                    watchdog();
                }
            }    
            if (ocheck){
                target   = spdata[0] ;
                tollo    = spdata[1] ;
                tolhi    = spdata[2] ;
//                greendiv = spdata[3] ;
            }
        }
        clrscr() ;
    }
    if(fcount) fwc=1;
    else       fwc=2;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
1 - Toggle auto/manual totalizing
*/
void    toggle1(void)
{
    if (oconti==1)  return ;                // continuous transmition
    if (oautom==0)  return ;                // autotalizing is disable
    clrscr() ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    if (fautom ^= 1)  puts("\n AUTO ") ;
    else  puts("\nMANUAL") ;
    pause(onesec) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
2 - Introduce the actual code number
*/
uchar  *bigcode(uchar *m, uchar *scode) ;
void    getcode(void)
{
    clrscr()  ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    if (acode != bigcode(" CODE ", acode))
    {
        if (!is_space(acode))  fwrite(acode) ;
        strcpy(acode, ascbuf) ; fread(acode) ;
        defpour() ;
        pdone() ;
    }
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}
uchar  *bigcode(uchar *m, uchar *scode)
{
    uchar   ret, n = 0, t, *p ;
    uchar   s[10]  ;
    bit     fleter = 0 ;            // for letter input
    bit     fclear = 0 ;            // for <C> key press
    bit     fstart = 0 ;            // for any key press

    strcpy(s, scode) ;
    if (strlen(s) == 0)  strcpy(s, "     0") ;
    if (strlen(s) <= 6)
    {
        strspace(ascbuf, 6 - strlen(s)) ;
        strcat(ascbuf, s) ;
        fexitcode = 0;
        _label(m, ascbuf) ; 
        if (fexitcode) return scode;
    }
    else
    {
        strspace(s, 6);
        sprintf(ascbuf, "%s%s%s", s, scode, s) ;
        while(!kbhit())
        {
            watchdog() ;
            if (!twait)
                switch(n)
            {
                case 0:  printf("\1%s", m) ; twait = hafsec ;
                    n = 1 ;
                    break ;
                case 1:  p = ascbuf + 6 ;
                    n = 2 ;
                    case 2:  memmove(s, p - 6, 6) ;
                    printf("\1%s", s) ; twait = 3;
                    if (!*p++) n = 0 ;
                }
        }
    }
    ungetch() ;
    printf("\n      ") ;
    n = 0 ;
    fscroll = true ;
    tblink = twosec ;
    while(true)
    {
        ret = kbhit() ? getch() : 0 ;
        switch(ret)
        {
            case  0  : watchdog() ;
                if (!tblink)
                {
                    if (testbit(fblink))
                    {
                        tblink = twosec ;
                        printf("\n%s", s) ;
                    }
                    else
                    {
                        fblink = 1 ;
                        tblink = hafsec ;
                        printf("\1%s", m) ;
                    }
                }
                break ;

            case 'P' : fleter = true ;
                break ;

            case 'Z' : fleter = false ;
                ret = '-' ;

                case '0' : if (testbit(fleter))  ret = 'A' ;
                case '1' : if (testbit(fleter))  ret = 'B' ;
                case '2' : if (testbit(fleter))  ret = 'C' ;
                case '3' : if (testbit(fleter))  ret = 'D' ;
                case '4' : if (testbit(fleter))  ret = 'E' ;
                case '5' : if (testbit(fleter))  ret = 'F' ;
                case '6' : if (testbit(fleter))  ret = 'G' ;
                case '7' : if (testbit(fleter))  ret = 'H' ;
                case '8' : if (testbit(fleter))  ret = 'I' ;
                case '9' : if (testbit(fleter))  ret = ret ;
                if (!fstart) printf("\n      ") ;  else
                    if (testbit(fblink))  printf("\n%s", s) ;
                if (testbit(fclear))  putchar(6) ;
                putchar(ret) ;
                ascbuf[n++] = ret ;
                ascbuf[n] = 0 ;
                sprintf(s, "%6s", (n<7)? ascbuf: ascbuf+n-6) ;
                tblink = tensec ;
                fstart = 1 ;
                break ;

            case 'C' : strcpy(s,"     0") ;
                printf("\n%s", s ) ;
                n = 0 ;
                tblink = twosec ;
                fblink = 0 ;
                fclear = 1 ;
                fstart = 1 ;
                fleter = 0 ;
                break ;

            case 'F' : if (fstart)
            {
                if (n > 12) n = 12 ; ascbuf[n] = 0 ;
                fscroll = false ;
                return(ascbuf) ;
            }

            default  : fscroll = false ;
            return(scode) ;
        }
    }
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
    3(ocheck = 1) - Define the checkweigher parameters
*/
void    define_cw(void)
{
    clrscr() ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    xbit(ftuse, 1) ;
    fblink = 0 ;
    if(fcount) fwc = 1;
    else       fwc = 2;
    target = getfvalue("TARGET", target, decim, 0) ;
    if (target){
        if (returncode) tollo = getfvalue("TOL-LO", tollo, decim, 0) ;
        if (returncode) tolhi = getfvalue("TOL-HI", tolhi, decim, 0) ;
//        if (returncode && ogreen) greendiv = getfvalue("GREEN ", greendiv, decim, 0) ;
    }
    else{
        tollo = tolhi = 0;//greendiv = 0 ;
    }
    pdone() ;
    xbit(ftuse, 0) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
    3(ocheck = 0) - Define the setpoint parameters
*/
void    sortsp(long *m, uchar n) ;
void    define_sp(void)
{
    uchar   i, n, t, val, j, r;
    long    old, set, esc ;
    uint    fminus=0;
    long    sphelp[spnum],sptem ;
    
    flag(fupl,0);
    clrscr() ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    xbit(fzenb, 1) ; xbit(fzuse, 1) ;
    xbit(ftuse, 1) ; xbit(fpuse, 1) ;
    for (t = 0 ; t < spnum ; t++) sphelp[t] = 0 ;
    old = 0 ;
    if(fcount) fwc=1;
    else       fwc=2;
    for (t = 0 ; t < spmax ; t++)
    {
        set = spdata[t] ;
        i = byte(set,0) - 1 ; byte(set,0) = (byte(set,1)&0x80)? 0xff: 0 ;
        if (!set) sphelp[i] = zeron ;
        else  sphelp[i] = set-old ;
        if (ospdef) old = set ;
    }
    r = setplim;
    if (r > 16) r = 16;
    for (t = 0 ; t < r ; t++)
    {
        if (t==6||t==7) continue ;  // bypass for relay 6, 7 and 8
        sprintf(secbuf, "SET-%02d", (uint)t+1) ;
        lab:        _printf("\n%s", secbuf) ;
        if (cdouble('F')) { t = r ; break ; }
        set = sphelp[t] ;
        fblink = 1 ;
        set = getfvalue(secbuf, set, decim, 0) ;
        if (fendset) t = r;
        if (set == -1)
        {            // P-P press
            puts("\nSET-  ") ;
            xbit(fpuse, 0) ; 
            val = getvalue(t+1, 16, 5, 2) ;
            xbit(fpuse, 1) ;
            if ((returncode)&&(val))
            {
                sprintf(secbuf, "SET-%02d",  (uint)val) ;
                t = val - 1 ;
            }
            goto lab ;
        }
        else sphelp[t] = set ;
        if (!returncode)
        {          // T press
            t ++ ;
            break ;
        }
    }
    n = old = 0 ;
    for (i = 0 ; i < t ; i++)
    {
        if (sphelp[i])
        {
            if (sphelp[i] == zeron) sphelp[i] = 0 ;
            if (ospdef) spdata[n] = old += sphelp[i] ;
            else spdata[n] = sphelp[i] ;
            byte(spdata[n++], 0) = i+1 ;
        }
    }
    if (!ospdef) sortsp((long *)spdata, n) ;
    spmax  = n ;
    for (t = n ; t < spnum ; t++) { spdata[t] = 0 ;}
        pdone() ;
    defpour() ;
    xbit(fpause0, 1) ;
    xbit(fpuse, 0) ; xbit(ftuse, 0) ;
    xbit(fzuse, 0) ; xbit(fzenb, 0) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}
ulong   retdig(long) ;
void    sortsp(long m[], uchar n)
{
    uint  i, j ;
    long  t ;

    for (i = 1 ; i < n ; i++)
        for (j = n-1 ; j >= i ; j--)
    {
        watchdog() ;
        if (retdig(m[j-1]) > retdig(m[j]))
        {
            t = m[j-1] ;
            m[j-1] = m[j] ;
            m[j] = t ;
        }
    }
}

ulong   retdig(long l)
{
    long t = l ;

    if  (byte(t, 1)&0x80) byte(t, 0) = 0xff ;
    else byte(t, 0) = 0 ;
    return(labs(t)) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
4 - Current subtotal and weighing order display
*/
void    subdisp(void)
{
    float   wtotal ;

    if (fcount)
    {
        wtotal = total/kunit*1000 ;         // in pcs
    }
    else
    {
        wtotal = total ;                    // in kg
    }
    clrscr()  ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    puts("\1 TOTAL") ; pause(onesec);
    if (wtotal >= 100000)   sprintf(ascbuf, "%6.0f", wtotal) ; else
        if (wtotal >= fpow(10, 6-decim)) fformat(ascbuf, wtotal) ; else
        if (decim)  sprintf(ascbuf, "%7.*f",(uint)decim, wtotal) ; else
        sprintf(ascbuf, "%6.0f", wtotal) ;
    printf("\1%s", ascbuf) ;
    if (fcount)  flag(fpcs, on) ;
    else        flag(fkilo, on) ;
    pause(twosec) ;
    if (fcount)  flag(fpcs, off) ;
    else        flag(fkilo, off) ;
    puts("\1 COUNT"); pause(onesec) ;
    sprintf(ascbuf, "\1%6d", order) ;
    puts(ascbuf) ;    pause(twosec) ;
    clrscr() ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
5 - Set counting mode by sample
*/
void    csample(void)
{

    uint  q = 0 ;
    uchar   sforce = 0 ,i,t;
    long set;
    float fset;
    if(fcount && kunit)
    {
        ranlo*=(float)kunit;
        ranhi*=(float)kunit;
    }

    if (!ocount)   return ;
    clrscr()  ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    flag(fpcs , 1) ;
    q = getfvalue(" COUNT", q, 0, 0) ;
    if (q)
    {
        puts("\n ==== ") ;
        twait = tensec ;
        fstab = weight0 = 0 ;
        if (dforce)
        {
            sforce = dforce ;
            dforce = 0 ; /* to avoid force zero influence */
        }
    }
    flag(fpcs, 0) ;
    while(twait)
    {
        if (testbit(fsamp))
        {
            atod = atodin() ;
            weight(atod) ;
            if (!fstab) tcount = twosec;
            if (!tcount)
            {
                weight0 -= tare ;
                if (weight0 <= zref)  twait = 0 ;
                else
                {
                    rfactor = (float)q/(weight0 - zref);
                    kunit = factor/rfactor ;
                    wstop = wwstop/kunit ;
                    kunit *= fpow(10, 3 - (int)wdecim) ;
                    round = 1 ;
                    decim = 0 ;
                    fcount = 1 ;
                    twait = 2 ;
                    break ;
                }
            }
        }
    }
    if ((!twait)&&(q))
    {
        puts("\n FAIL ") ; pause(onesec) ;
    }
    if (sforce) dforce = sforce ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
    fcount = !(kunit>0) ;
    toggle0();
    stackok();
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
6 - Set counting mode by uint weight
*/
void    unitwgt(void)
{
    float   funit;

    if(fcount && kunit)
    {
        ranlo*=(float)kunit;
        ranhi*=(float)kunit;
    }
    if (!ocount) return ;
    clrscr()  ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    fblink = 0 ;
    xbit(fzenb, 1) ;                                // decpoint movement
    funit = kunit + 0.1e-5 ;
    kunit1 = kunit;
///    texitfun = exitfun;
    kunit = getfvalue(" UNIT ", funit, 9, 0) ;         // decpoint = 9 (float type)
    xbit(fzenb, 0) ;
    if (kunit1 && fcount) fcounttoggle = 1;
    fcount = !(kunit>0) ;
    toggle0() ;
    fcounttoggle = 0;
    stackok() ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
7 - memory size in Kbytes
*/
void    membyte(void)
{
    uint m ;

    clrscr()  ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    m = ((uint)acode + memsize)/1024 ;
    sprintf(ascbuf, "\n %2d KB", m) ;
    label("\nMEMORY", ascbuf) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
8 - Display of current brute weight
*/
void    brutwgt(void)
{
    clrscr()  ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    printf("\n BRUTE") ; pause(onesec) ;
    if (fstab) wdisp = weight1 - zref;
    else wdisp = weight0 - zref;
    display(factorize(wdisp)) ;
    if (okilo)  flag(fkilo, fstab);
    if (opound) flag(fpound, fstab);
    pause(twosec) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
    stackok() ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F1 - set the date & time values


Description
ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒ
The MK41T56 TimeKeeper RAM is a low power 512-bit static CMOS RAM,
organized as 64 words by 8 bits. The first 8 bytes of the RAM are
used for the clock / calendar function  and are configured in BCD
format.
Address map of RAM :   0.  Seconds Register  ( 0 - 59 )
1.  Minutes Register  ( 0 - 59 )
2.  Hours   Register  ( 0 - 23 )
3.  Dayweek Register  ( 1 - 7  )
4.  Day     Register  ( 1 - 31 )
5.  Month   Register  ( 1 - 12 )
6.  Years   Register  ( 0 - 99 )
7.  Control Register
8.-63.  Free RAM

*/
#define _second s[0]
#define _minute s[1]
#define _hour   s[2]
#define _numday s[3]   /* week's day */
#define _day    s[4]
#define _month  s[5]
#define _year   s[6]
uchar           s[8] = {"       "} ;

uchar   day_of_week(struct date *sdate) ;
uchar   dec2bcd(uchar);
uchar   bcd2dec(uchar);

uchar   settime(void)
{
    uchar    t = 0 ;

    _second = dec2bcd(stime.ti_sec ) ;
    _minute = dec2bcd(stime.ti_min ) ;
    _hour   = dec2bcd(stime.ti_hour) ;
    _numday = day_of_week(&sdate) ;
    _day    = dec2bcd(sdate.da_day ) ;
    _month  = dec2bcd(sdate.da_mon ) ;
    _year   = dec2bcd(sdate.da_year%100) ;

    return(clockout(0, s)) ;
}
uchar   dec2bcd(uchar d)
{
    uchar s[2], ret ;

    sprintf(s, "%02d", (uint)d) ;
    ret  =  s[1] - '0' ;
    ret += (s[0] - '0') << 4 ;
    return  ret  ;
}
uchar   day_of_week(struct date *sdate)
{
    /*
    01-01-1990 - Monday (1)
    */
    uint    leap   ;
    uint    d, m[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } ;
    long    t = 0  ;

    for (d = 1990 ; d < sdate->da_year ; d ++ )
    {
        leap = (d % 4 == 0) && (d % 100 != 0) || (d % 400 == 0) ;
        if (leap) t += 366 ;
        else  t += 365 ;
    }
    leap = (d % 4 == 0) && (d % 100 != 0) || (d % 400 == 0) ;
    if (leap) m[1] = 29 ; else m[1] = 28 ;
    for (d = 01 ; d < sdate->da_mon ; d ++ )
    {
        t += m[d-1] ;
    }
    t += sdate->da_day ;
    d  =  t % 7 ;
    if (!d) d = 7 ;
    return( d ) ;
}
uchar   gettime(void)
{
    uchar    t = 7 ;
    uchar   *pt = s ;

    if (!clockin(s, 0))  return(0) ;
    stime.ti_sec   =  bcd2dec(_second) ;
    stime.ti_min   =  bcd2dec(_minute) ;
    stime.ti_hour  =  bcd2dec(_hour  ) ;
    sdate.da_week  =  bcd2dec(_numday) ;
    sdate.da_day   =  bcd2dec(_day   ) ;
    sdate.da_mon   =  bcd2dec(_month ) ;
    sdate.da_year  = (bcd2dec(_year  ) < 95) ? bcd2dec(_year) + 2000 :
    bcd2dec(_year) + 1900 ;
    return(1) ;
}
uchar   bcd2dec(uchar bcd)
{
    return (bcd >> 4) * 10 + (bcd&0x0f) ;
}

void    sscanf_date(uchar *date) ;
uint    controlDate(void) ;
void    sscanf_time(uchar *time) ;
uint    controlTime(void) ;
void    datime(void)
{
    uchar msg[9];
    uchar datetime[9], date[9], time[9];
    uchar ch;
    bit flag  = 0 ;
    bit fdate = 1 ;

    if (gettime()&&controlDate())
    {
        sprintf(date, "%02d.%02d.%02d", (uint)sdate.da_day,
                (uint)sdate.da_mon,
                (uint)sdate.da_year%100) ;
    }
    else
    {
        sprintf(date, "11.11.11") ;
        sprintf(time, "00.00.00") ;
    }
    twait = 0 ;
    sprintf(msg, "\n DATE ") ;
    sprintf(datetime, "%s", date) ;
    while(true)
    {
        switch(ch = kbhit()? getch(): 0x0)
        {
            case 0x0:
                watchdog() ;
                if (!twait)
                {
                    if (flag ^= 1) { twait = hafsec ; puts(msg) ; }
                        else
                    {
                        twait = trisec ;
                        printf("\n%s", datetime) ;
                    }
                }
                break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                datetime[0] = datetime[1];
                datetime[1] = datetime[3];
                datetime[3] = datetime[4];
                if (fdate)
                {
                    datetime[4] = datetime[6];
                    datetime[6] = datetime[7];
                    datetime[7] = ch;
                }
                else
                    datetime[4] = ch;
                twait = fullsec ;
                printf("\n%s", datetime);
                break;

            case 'T': return ;

            case 'F': if (fdate)
            {
                sscanf_date(datetime) ;
                sdate.da_year += (sdate.da_year < 95) ? 2000 : 1900 ;
                if (!controlDate()) { bell(2) ; break ; }
                    else settime() ;

                sprintf(msg,"\n TIME ") ; puts(msg) ;
                pause(hafsec) ;
                while(!kbhit())
                {
                    if (gettime()&&controlTime())
                        sprintf(time, "%02d.%02d.%02d",
                                (uint)stime.ti_hour,
                                (uint)stime.ti_min ,
                                (uint)stime.ti_sec ) ;
                    printf("\n%s", time) ;
                }
                if (getch() == 'F') strcpy(datetime, time) ;
                else
                {
                    ungetch() ;
                    fdate = 0 ;
                    strcpy(datetime, "00.00.00") ;
                    break ;
                }
            }
            sscanf_time(datetime) ;
            if (!controlTime()) { bell(2) ; break ; }
                if (settime())  pdone() ; else  puts("\nERROR ") ;
            return;

            case 'C':
                sprintf(datetime,"00.00.00") ;
                printf("\n%s", datetime) ;

                default : break;
        }
    }
}
void    sscanf_date(uchar *date)
{
    uchar  datetime[9] ;

    strcpy(datetime, date) ;
    datetime[2]  = datetime[5] = 0 ;
    sdate.da_day  = (uchar) atoi(datetime + 0) ;
    sdate.da_mon  = (uchar) atoi(datetime + 3) ;
    sdate.da_year = (uint ) atoi(datetime + 6) ;
}
uint    controlDate(void)
{
    uint    leap   ;
    uint    d, m[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } ;

    d = sdate.da_year ;
    leap = (d%4 == 0)&&(d%100)||(d%400 == 0) ;
    if (leap) m[1] = 29 ; else m[1] = 28 ;
    if (sdate.da_mon == 0)   return(false) ;
    if (sdate.da_mon > 12)   return(false) ;
    d = sdate.da_mon - 1 ;
    if (sdate.da_day == 0)   return(false) ;
    if (sdate.da_day > m[d]) return(false) ;
    return(true) ;
}
void    sscanf_time(uchar *time)
{
    uchar  datetime[9] ;

    strcpy(datetime, time) ;
    datetime[2]  = datetime[5] = 0 ;
    stime.ti_hour = (uchar) atoi(datetime + 0) ;
    stime.ti_min  = (uchar) atoi(datetime + 3) ;
    stime.ti_sec  = (uchar) atoi(datetime + 6) ;
    stime.ti_hund =  0 ;
}
uint    controlTime(void)
{
    if (stime.ti_hour > 23)  return(false) ;
    if (stime.ti_min  > 59)  return(false) ;
    if (stime.ti_sec  > 59)  return(false) ;
    return(true) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F2 - Set the zero values range
*/
void    ranges(void)
{
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    xbit(ftuse, 1) ;
    fblink = 0 ;
    zerodiv = getfvalue(" 2ERO ", zerodiv, decim, 0) ;
    if (returncode)
    {
        xbit(fzenb, 1) ; xbit(fzuse, 1) ;
        zeroneg = getfvalue("-2ERO-", zeroneg, decim,0) ;
        xbit(fzenb, 0) ; xbit(fzuse, 0) ;
    }
    else zeroneg = 0 ;
    pdone() ;
    xbit(ftuse, 0) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F3 - Set the low/high ranges for auto totalizing
*/
void    ranset()
{
    ranlo = getfvalue("LORAN ", ranlo, decim, 0) ; if (returncode)
    ranhi = getfvalue("HIRAN ", ranhi, decim, 0) ;
    pdone() ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F7  - Erase a code from memory
*/
void    delete(void)
{
    uchar  t  ;
    uchar *p  ;

    p = bigcode(" ERASE", acode) ;
    if (atof(p))
    {
        if  (p == acode)
        {                  // actual code
            ferase(ascbuf) ;
            kunit  = acode[0] = 0 ;
            order  = total  = 0 ;
            gorder = gtotal = 0 ;
            target = tollo  = tolhi = 0 ;
            spmax  = 0 ;
            for (t = 0 ; t < spnum ; t++)  spdata[t] = 0 ;
            pdone()  ;
        }
        else
        {
            if (ferase(ascbuf)) pdone() ;
            else
            {
                puts("\nNOCODE") ; pause(onesec) ;
            }
        }
    }
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F9  - Mcode area status (free/busy by codes)
*/
void    smcode(void)
{
    uchar   s[8] = {"FRE.COD"}, d[8] ;
    uint    free, busy ;

    if (acode[0])  fwrite(acode) ;
    busy = fquantity() ;
    free = marea/(dp+6*4) - busy ;  // assume, that on average spmax = 6
    sprintf(d, "%3d.%3d", free, busy) ;
    label(s, d) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F11 - Setpoint's long times definition
*/

void    splong(void)
{
    uint    set, t ;

    if (!ocheck)
    {
        rel8off = cdelay("8 OFF ", rel8off, 999) ;
        return ;
    }
    clrscr() ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    xbit(ftuse, 1) ;
    for (t = 0 ; t < splim ; t++)
    {
        sprintf(secbuf, "REL-%02d", (t) ? t : relayZ) ;
        _printf("\n%s", secbuf) ;
        if (cdouble('F'))
        {
            t = splim ;
            break ;
        }
        set = sptime[t] ;
        fblink = 1 ;
        set = getfvalue(secbuf, set, 1, 999) ;
       sptime[t] = set ;
        if (!returncode)
        {                  // T press
            t ++ ;
            break ;
        }
    }
    while(t < splim) sptime[t++] = 0 ;
    pdone() ;
    xbit(ftuse, 0) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F12 - Dead weight zone for setpoints
*/
void    _dload(void)
{
    if (ocheck)      return ;               // checkweigher mode
    if (!spdata[0])  return ;               // no setpoints
    dzone = getfvalue("D-LOAD", dzone, decim, 0) ;
    pdone() ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F8, F10, F13 - Relcon's block delays
*/
uint    cdelay(uchar *s, uint d, ulong max)
{
    d = d * 10 / onesec ;
    d = getfvalue(s, d, 1, max) ;
    d = d * onesec / 10 ;
    pdone( ) ;
    return d ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F14 - Manual test of setpoints
*/
#define workstage       0x01
#define restart         0x02
void    manual_sp(void)
{
    uint    rel = 0 ;
    uchar   ret = 0 ;

    fstart = workstage ;
    printf("\nREL 00") ;
    while(true)
    {
        watchdog() ;
        if (kbhit())
        {
            switch(ret = getch())
            {
                case 'F' :  ret = undisp(5) ;
                    if (ret)  rel ^= 1<<(ret-1) ;
                    else
                    {
                        keystatus = fstart = 0 ;
                        return ;
                    }
                    break ;
                default  :  if (!isdigit(ret)) break ;
                disflag[4] = disflag[5] ;
                printf("\6%c", ret) ;
                if (undisp(5) > spnum)
                {
                    cursor = 5 ;
                    puts("0") ;
                }
                ret = undisp(5) ;
                break ;
                case 'C' :  cursor = 5 ;
                    puts("00") ;
                    ret = 0 ;
                    rel = 0 ;
                }
            if ((ret)&&(rel&(1<<(ret-1)))) puts("\4-") ;
            else  puts("\4 ") ;
        }
        keystatus = restart ;
        relcom(rel, 1) ;
    }
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F98 - Clear of all sub [and grand] totals
*/
void    clear_tot(bit and)
{
    struct  mdat *ptr ;
    uchar t = 0, *pt1 ;

    if (!and)
    {
        clrscr() ; flag(fmidl, 1) ; flag(fmidr, 1) ;
    }
    puts("\nCLEAR ");
    twait = trisec ;
    while(twait)
    {
        if (keyboard() == 'F')
        {
            t ++ ;
            if (t == 1) twait = hafsec ;
            else  break ;
        }
        watchdog() ;
    }
    if (t == 2)
    {
        ptr = (struct mdat *)&mcode ;
        while(ptr < meof)
        {
            if (ptr->md_ferase == ' ')
            {
                ptr->md_order  =  0  ;
                ptr->md_total  =  0  ;
                if (and)
                {
                    ptr->md_gorder =  0  ;
                    ptr->md_gtotal =  0  ;
                }
            }
            pt1  = (uchar *)ptr ;
            pt1 += dp + ptr->md_spmax * 4 ;
            ptr  = (struct mdat *)pt1 ;
        }
        order = total = 0 ;
        if (and) gorder = gtotal = 0 ;
        pdone() ;
    }
    if (!and)
    {
        flag(fmidl, 0) ; flag(fmidr, 0) ;
    }
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F99 - Clear of all codes from 'mcode' memory
*/
void    clear_all(void)
{
    uchar t = 0 ;

    puts("\nCLEAR ");
    twait = trisec ;
    while(twait)
    {
        if (kbhit())
        {
            if (getch() == 'F')
            {
                if (++t == 1) twait = hafsec ;
                else  break ;
            }
        }
        else watchdog() ;
    }
    if (t == 2)
    {
        meof    = &mcode ;                   // pointer to mcode 'eof'
        order   = total   = acode[0] =  0 ;
        gorder  = gtotal  = 0 ;
        spmax   = dzone   = dtime = dtimez = 0 ;
        zerodiv = zeroneg = 0 ;           // zero range for relayZ(ero)
        for (t = 0; t < spnum; t++) spdata[t] = 0 ;
        target = tollo = tolhi = 0 ;
        kunit = 0L;
        toggle0();
        pdone() ;
    }
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
'done' screen message
*/
void    pdone(void)
{
    puts ("\n DONE ") ;
    pause(onesec) ;
}
void    blink(uchar *s, uchar t)
{
    twait = t ;
    tblink = fblink = 0 ;
    while(twait)
    {
        watchdog() ;
        if (tblink == 0)
        {
            tblink = hafsec ;
            fblink ^= 1 ;
            if (fblink)  puts("\n      ") ;
            else  printf("\n%6s", s) ;
        }
    }

}
void    manual_bcd(void)
{
    xdata ulong bcdval=0,tempstop;
    if (!bcd)  return ;               // no bcd
    clrscr()  ;
    if (bcd420) tempstop=10*fpow(10,decim);
    else tempstop=wstop;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    do
    {
        if(bcd420)
        {
            bcdval = getfvalue("4 - 20", bcdval, decim, 0);
        }
        else bcdval = getfvalue(" BCD  ", bcdval, decim, 0) ;
        if (bcd)
        {                                 // bcd communication
            if (bcdval >(long)tempstop && bcd420) bcdval = tempstop ;
            if (bcd420)
            {                          // 4-20mA format
                if (fover|fsign) imrel1 = bcdini ? 0 :-1 ;
                else imrel1 = 0x0fff*bcdval/tempstop ;
            }
            else
            {
                if (bcdout)
                {                      // binary format
                    if (bcdsel)
                    {
                        imrel1 = 0x7fff*bcdval/tempstop ;
                    }
                    else
                    {
                        if (fover|fsign) imrel1 = bcdini ? 0 :-1 ;
                        else imrel1 = 0xffff*bcdval/tempstop ;
                    }
                }
                else
                {                             // bcd format
                    imrel1 = dec4bcd(bcdval) ;
                }
            }
            if (bcdpol)
                if ((bcdsel)||(!fover)) imrel1 = ~imrel1 ;
            relset(imrel1) ;          // new relbox, routine in tigerc.c
        }
        if(bcdval==0)
        {
            pdone();
            flag(fmidl, 0) ; flag(fmidr, 0) ;
            return;
        }
    }
    while(true);
}


void  relontimers(void)
{
    uint    set, t ;

    if (!ocheck) return ;
    clrscr() ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    xbit(ftuse, 1) ;
    for (t = 0 ; t < splim ; t++)
    {
        sprintf(secbuf, "REL-%02d", (t) ? t : relayZ) ;
        _printf("\n%s", secbuf) ;
        if (cdouble('F'))
        {
            t = splim ;
            break ;
        }
        set = rel3on[t] ;
        fblink = 1 ;
        set = getfvalue(secbuf, set, 1, 999) ;
        rel3on[t] = set ;
        if (!returncode)
        {                  // T press
            t ++ ;
            break ;
        }
    }
    while(t < splim) rel3on[t++] = 0 ;
    pdone() ;
    xbit(ftuse, 0) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}
// ‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹
/*
F18 - Setpoint's long times definition
*/

void    splong1(void)
{
    uint    set, t ;

    if (!ocheck)
    {
        rel8off = cdelay("8 OFF ", rel8off, 999) ;
        return ;
    }
    clrscr() ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    xbit(ftuse, 1) ;
    for (t = 0 ; t < splim1 ; t++)
    {
        sprintf(secbuf, "REL-%02d",  t+1 ) ;

        _printf("\n%s", secbuf) ;
        if (cdouble('F'))
        {
            t = splim ;
            break ;
        }
        set = sptime1[t] ;
        fblink = 1 ;
        set = getfvalue(secbuf, set, 1, 999) ;
        sptime1[t] =set ;
        sprintf(ascbuf,"%d %d %d \n",(uint)sptime1[0],(uint)sptime1[1],(uint)sptime1[2]);
        comstring(ascbuf);
        if (!returncode)
        {                  // T press
            t ++ ;
            break ;
        }
    }
    while(t < splim) sptime1[t++] = 0 ;
    pdone() ;
    xbit(ftuse, 0) ;
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}