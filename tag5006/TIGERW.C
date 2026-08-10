#include "tiger.h"
/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³                    Weighing  module                    ³Û
³                   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ                   ³Û
³  Digital filter,  zero tracking,  drift compensation   ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

void    weight(uint w) small
{
    if (!tfil)
    {
        tfil = filrate;
        if (filcof<filmax) filcof++;
    }
    stable(filter(&weight0, w));
    zerohandler();
    if (!ftest){
        if (treqt){
            if (omant && !wdisp && flag(fzero,test) && !fcount){
                manualtare(1) ;
            }
            else{
                if(!otarof){
                    if (ocumt) cumulatare() ;
                    else       toggletare() ;
                }
            }
        }
        if(ozerotrace)
        {
            if (wdisp>halfdiv)drift();
        }
        else
            drift();
        if (dforce) forcezero(weight0 - zref) ;
    }
    if (fstab) weight3 = weight1 - zref ;
    else weight3 = weight0 - zref ;
    myw=wdisp = factorize(weight3-tare) ;
    if (!ftest) wlimit();
}

char    filter(ulong *n, uint w) small
{
    ulong   m;
    word(m, 1) = w;
    byte(m, 0) = byte(m, 3) = 0;
    m -= *n;
    m >>= filcof;
    if (byte(m, 0)) byte(m, 0) = -1;
    *n += m;
    byte(m, 1) ^= byte(m, 0);
    byte(m, 2) ^= byte(m, 0);
    if (byte(m, 1)|byte(m, 2))
    {
        byte(m, 0) |= 1;
        tstab = twosec;
    }
    else byte(m, 0) = 0;
    return (byte(m,0));
}

void    stable(char c) small
{
    ulong   n;
    if (fstab)
    {
        n = weight0 - weight1;
        if (byte(n, 0)) ramp--;
        else ramp++;
        byte(n, 1) ^= byte(n, 0);
        byte(n, 2) ^= byte(n, 0);
        byte(n, 0) ^= ramp;
        if ((!byte(n,1))&&(!byte(n,2))) byte(n,0) = -1;
        if (byte(n,0)&0X80) ramp = 0;
        else if ((ramp>stabof)||(ramp<-stabof))
        {
            filcof = ramp = 0;
            fstab = 0;
            tfil = filrate;
            tgostab = stabin;
        }
    }
    else
    {
        tstab = twosec;
        if (c)
        {
            ramp += c;
            tgostab = stabin;
            if ((ramp^c)&0X80) ramp = 0;
            if ((ramp>stabout)||(ramp<-stabout))
            {
                ramp = filcof = 0;
                tfil = filrate;
            }
        }
        else
        {
            ramp = 0;
            if (!tgostab)
            {
                fstab = 1;
                weight1 = weight0;
            }
        }
    }
}
void    zerohandler() small
{
    ulong   n;
    if (fstab)
    {
        if ((treqz)&&(!ftest))
        {
            if (!ofulz && !ftranzero)
            {         // Restriction for zeriong
                if (omehal)
                {              // No zeroing for container (op56 = on)
                    treqz = 0 ;
                    return ;

                }
                if (onist)
                {         // For NIST standart zref <= 20div
                    if (wdisp+tare > twentydiv)
                    {
                        treqz = 0 ;
                        return ;
                    }
                }
                else
                {           // zref <= 4% of 'stop'
                    if (wdisp+tare > fourperstop)
                    {
                        treqz = 0 ;
                        return ;
                    }
                }
            }
            if (tstab<onesec)
            {
                if (transzero) transzero = 2 ;  // zero request from computer
                flag(fzero, on);
                znext = zref = lastzref = weight0;
                weight1 = weight0;
                wtrack = 0;
                ftrack = 1;
                frange = 1;
                ttrack = trisec;
                treqz = 0;
            }
        }
        else
        {
            n = weight0 - zref;
            if (byte(n,1)^byte(n,0)) flag(fzero, off);
            else
            {
                byte(n, 2) ^= byte(n, 0);
                byte(n, 3) ^= byte(n, 0);
                if (word(n,2)<quartdiv) flag(fzero, on);
                else flag(fzero, off);
            }
        }
    }
    else flag(fzero, off);
    ftranzero = 0;
}

void    forcezero(long weight)
{
    bit ret ;

    if (oneg) ret = (weight < wforce) ;
    else ret = (weight>-(long)wforce)&&(weight<(long)wforce) ;
    if ((ret)&&(factorize(weight)))
    {
        if (phase == 0)
        {
            if (mforce < weight)
            {
                mforce = weight ;
                tforce = dforce ;
            }
            else if (!tforce)
            {
                flag(fzero, on) ;
                znext = zref = lastzref = weight0 ;
                weight1 = weight0 ;
                mforce = 0 ;
                wtrack = 0 ;
                ftrack = 1 ;
                frange = 1 ;
                ttrack = trisec ;
            }
        }
        else
        {
            if (mforce <= weight)
            {
                tforce = dforce ;
                phase = 0 ;
            }
            mforce = weight ;
        }
    }
    else
    {
        mforce = wforce ;
        phase = 1 ;
    }
}

void    manualtare(uint n)
{
    long   val = 0 ;
    uchar  ret = true ;

//    if (oprofi) return;
    treqt = 0 ;
    flag(fzero, 0) ;
    flag(ftare, 0) ;
    flag(fmidl, 1) ;
    flag(fmidr, 1) ;
    xbit(ftuse, 1) ;
    fblink = 0 ;
    while(ret)
    {
        if (n==1){  
///            texitfun = exitfun;
            val = getfvalue(" TARE ", val, decim, 0) ;
        }
        if (n==0){  
///            texitfun = exitfun;
            val = getfvalue("TARE2 ", val, decim, 0) ;
        }
        if ((ofult)||(val<=fiveperstop))
        {
            tare = (float)val/factor ;
            ret  =  false ;
        }
        else  message("NOT VALID", 2) ;
    }
    xbit(ftuse, 0) ;
    clrscr() ;
}

void    cumulatare()
{
    long    ttare, wtare, set ;

    if (tstab<onesec)
    {
        if (transtare) transtare = 2 ;                 // remote tare request
        treqt = 0 ;
        ttare = weight1 - zref ;
        wtare = factorize(ttare) ;
        if ((ofult)||(wtare<=fiveperstop))
        {
            tare = ttare ;
            if ((!ocheck)&&(xbit(fpour,2))&&(spmax))
            { // for EMPTY check
                set = spdata[spmax-1] ;                // in pouring setpoint
                byte(set,0) = (byte(set,1)&0x80)? 0xff: 0 ;
                if (wtare <= labs(set)){
                    puts("\n EMPTY") ; pause(twosec) ;
                    xbit(fempty, on) ; tare = 0;
                }
                else xbit(fempty, 0) ;
            }
            if (byte(tare,0)) tare = 0;
            if (tare<onediv)  tare = 0;
            if ((osptar)&&(tare)) xbit(xtare, on) ;  // relays start after tare
        }
    }
}

void    toggletare()
{
    long   wtare, set ;

    xbit(fempty, 0);
    if (oricun && osptar){
        set = spdata[spmax-1];
        byte(set,0) = (byte(set,1)&0x80)? 0xff: 0 ;
        wtare = factorize(weight1 - zref);
        if (wtare <= labs(set) && !flag(ftare,test)){
            xbit(fempty, on);
            puts("\n EMPTY"); 
            pause(twosec); fstart = 0;
        }
    }

//    if((!otarof || ftranstare) && !xbit(fempty, test)){
    if(ftranstare || !xbit(fempty, test)){
        if (flag(ftare, test)){
            if (transtare) transtare = 2 ;       // remote tare request
            treqt = 0;
            tare = 0;
        }
        else 
            if ((tstab < onesec) || (oricun && osptar)){
                if (transtare) transtare = 2 ;   // remote tare request
                treqt = 0;
                if ((ofult)||(wdisp<=fiveperstop)){ // Restriction for tare
                    tare = weight1 - zref;
                    if (byte(tare,0)) tare = 0;
                    if (tare<onediv)  tare = 0;
                    if (osptar) xbit(xtare, on) ;    // relays start after tare
                }
        }
    }
    else{
        treqt = 0;
    }
    ftranstare = 0;
}

long    factorize(long x)
{
    float   fx ;
    ulong  rnd ;
    uchar  swt ;

    fx = x ;
    if (ftest) fx *= factor ;
    else
    {
        if (fcount) swt = 1 ; else
            if (othree)
        {                       // triple rounding
            if (fx <= onefifthwstop) swt = 3 ; else
                if (fx <= twofifthwstop) swt = 2 ; else
                swt = 1 ;
        }
        else
            if (oduble)
        {                               // double rounding
            if (fx <= onetenthwstop) swt = 2 ; else
                swt = 1 ;
        }
        else                     swt = 1 ;      // normal rounding
        switch(swt)
        {
            case 1: rnd = round ;
                fx *= rfactor ;
                break ;
            case 2: rnd = twofifthround ;
                fx *= factor/rnd ;
                break ;
            case 3: rnd = onefifthround ;
                fx *= factor/rnd ;
            }
    }
    if (byte(fx,3)&0X80) fx -= 0.5;
    else fx += 0.5;
    x = (long) fx;
    if (!ftest) x *= rnd;
    return (x);
}

void    drift() small
{
    ulong   w;
    w = weight0 - zref;
    if (ftrack)
    {
        w -= wtrack;
        if ((byte(w,1)^byte(w,0))==0)
        {
            byte(w, 2) ^= byte(w, 0);
            byte(w, 3) ^= byte(w, 0);
            if (((uint) w)<halfdiv)
            {
                if (!ttrack)
                {
                    byte(w, 3) ^= byte(w, 0);
                    byte(w, 2) ^= byte(w, 0);
                    w += zref;
                    zref = znext ;
                    if (ozmem) lastzref = zref ;
                    znext = w;
                    if (frange) ttrack = trisec;
                    else ttrack = fullsec;
                }
                byte(w, 0) = 0;
            }
            else byte(w, 0) = 1;
        }
        else byte(w, 0) = 1;
        if (byte(w, 0))
        {
            ftrack = 0;
            znext = zref;
            ttrack = twosec;
        }
    }
    else if (!(ttrack|tstab))
    {
        wtrack = w;
        frange = (byte(w,1)==byte(w,0));
        if (frange)
        {
            byte(w, 2) ^= byte(w, 0);
            byte(w, 3) ^= byte(w, 0);
            frange = (((uint) w)<halfdiv);
            if (frange) wtrack = 0;
        }
        if ((!frange)&&(ftare))
        {
            w = wtrack - tare;
            frange = (byte(w,1)==byte(w,0));
            if (frange)
            {
                byte(w, 2) ^= byte(w, 0);
                byte(w, 3) ^= byte(w, 0);
                frange = (((uint) w)<halfdiv);
                if (frange) wtrack = tare;
            }
        }
        ftrack = frange | hopdrift;
        if (frange) ttrack = trisec;
        else ttrack = fullsec;
        if (!ftrack) ttrack = twosec;
    }

}

void    wlimit() small
{
    long    n ;

    fsign = (bit) (byte(wdisp, 0)&0X80);
    if (fsign)
    {
        if (oneg)
        {
            n = zref - weight0 ;
            if ((factorize(n)>0)&&(tare))
            {
                fover = true ;
                timeout = 0 ;
            }
            else fover = (~(bit)(tare>0)) ;
        }
    }
    else
    {
        fover = true ;
        n = weight0 - zref0 ;

        if (n > astop) xputs(" STOP ") ; else
            if (wdisp > 999999) xputs("OV-FLO") ; else
            fover = false ;
/*
        fover = true ;
        n = weight0 - zref0 ;
        if (n > astop) puts("\1 STOP ") ; else
            if (wdisp > 999999) puts("\1OV-FLO") ; else
            fover = false ;
*/
    }
    if (fover)
    {
        if (fsign)
        {
            wdisp = 0;
            if (timeout) fover = 0;
            else
            {
                if (atod < a2dlim) errors(4, 0) ;
//                puts("\1------");

                xputs("\1------");

                if (!ofulz)
                {
                    if (tstab<onesec)
                    {
                        flag(fzero, on) ;
                        znext = zref = lastzref = weight0 ;
                        weight1 = weight0 ;
                        mforce = 0 ;
                        wtrack = 0 ;
                        ftrack = 1 ;
                        frange = 1 ;
                        ttrack = trisec ;
                    }
                }
                if (otoneg) treqz = hafsec;
            }
        }
    }
    else timeout = hafsec;
}
