#include "tiger.h"
/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³          Checkweigher and setpoints processing           ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/
void    checkweigher(long weight)
{
    uint  t, ret ;

    if((target == 0)||(fdir == 0))  return ;
    if (weight <= zerodiv) t = 1 ;            else
        if (weight <  target - tollo )   t = 2 ;  else
        if (weight <  target - greendiv) t = 3 ;  else
        if (weight <= target + greendiv) t = 4 ;  else
        if (weight <= target + tolhi )   t = 5 ;  else
        t = 6 ;
    if (spind == t)  return ;
    ret = spret ;
    switch(spind = t){
        case 1 :
                t = none ;
                spret  = 0x40;
                break ;
        case 2 :
                if (olight)
                    if (greendiv) { 
                        spret = 0x000 ; t = none ; 
                    }else{
                        spret = 0x101 ;
                        t = yellow ;
                    }else{
                        spret = 0x101 ;
                        t = yellow ;
                    }
                break ;
        case 3 :
                if (olight)
                    if (greendiv) { 
                        spret = 0x101 ; t = yellow ; 
                    }else{
                        spret = 0x202 ;
                        t = green ;
                    }else{
                        spret = 0x101 ;
                        t = green|yellow ;
                    }
                break ;
        case 4 :
                t = green ;
                spret  = 0x202 ;
                break ;
        case 5 :
                if (olight)
                    if (greendiv) { 
                        spret = 0x404 ; t = red ; 
                    }
                    else{
                        spret = 0x202 ;
                        t = green ;
                    }
                    else{
                        spret = 0x404 ;
                        t = green|red ;
                    }
                break ;
        case 6 :
                if (olight)
                    if (greendiv) { 
                        spret = 0x000 ; t = none ; 
                    }
                    else{
                        spret = 0x404 ;
                        t = red ;
                    }
                    else{
                        spret = 0x404 ;
                        t = red ;
                    }
                break ;
    }
    light(t) ;
    if (spret == ret) return ;
    fdelay = 1 ;
    if (!spret) tdelay = 0 ;
    else
        if (spret == 0x40) tdelay = 0 ;
        else{
            tdelay = dtime;
        }
    if (tdelay){
        if (imrel & 2) imrel = spret & 0xf00 | 2;
        else           imrel = spret & 0xf00;
    }
    
    switch(spret&0xff){
        case 0x40 : t = 0 ; break ;         // relay 7
        case 0x04 : t = 3 ; break ;         // relay 3
        default   : t = spret&0xf ;         // relay 1,2
    }
   
    if (!sptime[t]){ 
        flife = 0 ;
    }
    else{
        flife = 1 ;
        nlife = (t) ? t : relayZ ;          // relay's number
        tlife = sptime[t] + tdelay ;        // relay's longlife timer        
    }

    //if (rel3on[t])
    {
        drel3on = rel3on[t]; 
        oldimrel = spret;
//sprintf(ascbuf,"\n33 %x  %d  %d",spret,(uint)drel3on,(uint)t); comstring(ascbuf); while(fsend) watchdog();
    }
}

void    fsetpoint(long weight)                  // filling setpoint
{
    uint   t = 0;
    long   set ;
    uchar  cb[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0};


    if (osptar)
    {
        if(oforcetar) if(!xbit(xtare, 2)) return ;
        if (weight < zeroneg)
        {
            xbit(xtare, off) ; imrel = 0 ;
            xbit(fpause0, 1) ; return ;
        }
    }

    if ((weight <= zerodiv) || frestart2)
    {
        if (xbit(fpause0, off) || frestart2)
        {
            xbit(fpause8, off) ;
            tdelay0 = dtime0 ;
            if (osprev)
            {   
                if(!specrel) spret = 0xff7f ;       // reverse setpoint
                if( specrel) spret = 0xff5f ;       // reverse setpoint with special rel 6
            }
            else{ 
                spret = 0 ;                // normal setpoint
            }
            imrel = 0 ;
            spind = frestart2 = 0 ;
            trelay = 255 ;                  // 0.75 second
        }
        spret |= relbit(relayZ) ;
        imrel |= relbit(relayZ) ;
    }
    else
    {
        spret &= ~relbit(relayZ) ;
        imrel &= ~relbit(relayZ) ;
    }

    if (fdir >= 0)
    {                        // up
        for (t = spind ; t < spmax ; t++)
        {
            set = spdata[t] ;
            byte(set, 0) = 0 ;
            if (weight < set) goto _bottom ;

            if(ospord && !osprev)// op 38 && 37
            {
                if (!tdelay0) spret ^= relbit(byte(spdata[t], 0)) ;
                imrel  = spret ;
                /*****************************/
                if(opuls)
                {
                    if (t>=spmax-1)
                        if(osprev)
                    {
                        spret &= ~cb[spmax-1];
                    }
                    else
                    {
                        spret |= cb[spmax-1];
                        spret ^= cb[spmax-1];
                    }
                }
                /*******************************/
                tdelay = dtime ;
            }
            else
            {                
                if (t > 0)
                {
                    if (ospord)
                    {
                        if (!tdelay0) spret ^= relbit(byte(spdata[t-1], 0)) ;
                        imrel  = spret ;
                    }
                    /*****************************/
                    if(opuls)
                    {
                        if (t>=spmax-1)
                            if(osprev)
                        {
                            spret &= ~cb[spmax-1];
                        }
                        else
                        {
                            spret |= cb[spmax-1];
                            spret ^= cb[spmax-1];
                        }
                    }
                    /*******************************/
                }
            }
            if(ospord && !osprev)
            {
                if (!tdelay0) spret ^= relbit(byte(spdata[t+1], 0)) ;
            }
            else{
                spret ^= relbit(byte(spdata[t], 0)) ;
            }
            t4flag=0;
            t5flag=1;
            t5=0;
            t4=0;
        }
        if (osp8)
            if (xbit(fpause8, test))
        {
            if (!twait)
            {
                if (!onostab && ototalrel8){
                    if (fstab){
                        xbit(fpause0, on) ; 
                        imrel = spret |= 0x80 ;
                        if (osptar) xbit(xtare, off) ;
                    }
                }else{
                    xbit(fpause0, on) ; 
                    imrel = spret |= 0x80 ;
                    if (osptar) xbit(xtare, off) ;
                }
            }
            return ;
        }
        else
        {
            twait = dtime8 ;
            xbit(fpause8, on) ;
        }
    }
    else
    {                                  // down, dzone - deadweight zone
        for (t = spind ; t > 0 ; t--)
        {
            set = spdata[t-1] ;
            byte(set, 0) = 0 ;
            if (weight >= set - dzone) { 
                spind = t ;
                return ; 
            }
            if(ospord && !osprev){
                if (!tdelay0) spret ^= relbit(byte(spdata[t-1], 0));
            }
            else{ 
                spret ^= relbit(byte(spdata[t-1], 0));
            }
            /*****************************/
            if(opuls)
            {
                if (t>=spmax-1)
                    if(osprev)
                {
                    spret |= cb[spmax-1];
                }
                else
                {
                    spret |= cb[spmax-1];
                    spret ^= cb[spmax-1];
                }
            }
            /*******************************/
            if(ospord && !osprev){
                if (!tdelay0) spret ^= relbit(byte(spdata[t], 0));
            }
            else
            {
                if ((ospord)&&(t>1))
                {
                    spret ^= relbit(byte(spdata[t-2], 0)) ;
                }
            }
            xbit(fpause8, off) ;
            tdelay = 2 ;
            fdelay = 1 ;
            t4flag=0;
            t5flag=1;
            t4=0;
            t5=0;
        }
    }
    _bottom :
    fdelay = 1 ;
    spind  = t ;
    /*****************/
    if(opuls)
    {
        if(t>=spmax-1)
        {
            if(!t5 && t4flag==1 && !t4)
            {
                spret ^= relbit(byte(spdata[t], 0)) ;
                t4flag=0;
                t5flag=1;
                t4=dpuls;
            }
            if(!t4 && t5flag==1 && !t5)
            {
                spret ^= relbit(byte(spdata[t], 0)) ;
                if(!ts4flag)t4=dpuls;
                ts4flag=1;
                t4flag=1;
                t5flag=0;
                t5=dpulsl;
            }
            imrel  = spret ;
        }
    }
    /******************/
}

void    psetpoint(long weight)                  // pouring setpoint
{
    uint    t ;
    long    set  ;

    if (xbit(fempty, test))  return ;
    if (weight == 0)
    {
        if (xbit(fpause0, off))
        {
            xbit(fpause8, off) ; relay1on() ;
            if (osprev) spret = 0xff7f ;
            else spret = 0 ;
            spind = 0 ;
            imrel = 0 ;
        }
    }
    if (fdir <= 0)
    {                        // down
        for (t = spind ; t < spmax ; t++)
        {
            set = spdata[t] ;
            byte(set, 0) = (byte(set, 1)&0x80)? 0xff: 0 ;
            if (weight > set) goto _pottom ;
            if (t > 0)
            {
                if (ospord)
                {
                    spret ^= relbit(byte(spdata[t-1], 0)) ;
                    imrel  = spret ;
                }
                tdelay = dtime ;
            }
            spret ^= relbit(byte(spdata[t], 0));
        }
        if (osp8)
            if (xbit(fpause8, test))
        {
            if (!twait)
            {
                if (!onostab && ototalrel8){
                    if (fstab){    
                        xbit(fpause0, on) ; imrel = spret |= 0x80 ;
                        if (osptar)  xbit(xtare, off) ;
                    }
                }else{
                     xbit(fpause0, on) ; imrel = spret |= 0x80 ;
                     if (osptar)  xbit(xtare, off) ;
                }
            }
            return ;
        }
        else
        {
            xbit(fpause8, on) ; twait = dtime8 ;
        }
    }
    else
    {                                  // up, dzone - deadweight zone
        for (t = spind ; t > 0 ; t--)
        {
            set = spdata[t-1] ;
            byte(set, 0) = (byte(set, 1)&0x80)? 0xff: 0 ;
            if (weight <= set + dzone) { spind = t ; return ; }
            spret ^= relbit(byte(spdata[t-1], 0));
            if ((ospord)&&(t>1))
                spret ^= relbit(byte(spdata[t-2], 0)) ;
            xbit(fpause8, off) ;
            tdelay = 0 ;
            fdelay = 1 ;
        }
    }
    _pottom :
    fdelay = 1 ;
    spind  = t ;
}

uint    relbit(uchar n)
{
    if (n) return(1<<n-1) ;
    else   return(0) ;
}

void    relay1on(void)
{
    relset(imrel | 0x01) ;
    trelay = 40 ;
    while(trelay) watchdog() ;
}

void    defpour(void)
{
    if ((!ocheck)&&(spmax))
    {
        pass  = byte(spdata[0], 1) ;
        pass |= byte(spdata[1], 1) ;
        pass &= 0x80 ;
        xbit(fpour, pass > 0) ;
        fstart=0;
    }
}

#define stopstage       0x00
#define workstage       0x01
#define selflatch       0x02

#define stopkey         0x04
#define restart         0x02
#define start           0x01

void    getkey(void) ;
void    relcom(uint imrel, uchar l)
{
    uchar t, f;
    uint  s;

//sprintf(ascbuf,"\n %d",(uint)fstart); comstring(ascbuf); //while(fsend) watchdog();
    if (((keystatus & start) || (keystatus & restart)) && oricun) 
        if (!flag(ftare,test)){
            if (oricun && osptar) toggletare();
            else                  treqt = 30;
        }

    if (!bytest) {         // relay status tramsmission
        if (!l){
            if (keystatus & stopkey){
                fstart = stopstage ;
                if (oricun && osptar && flag(ftare,test)) toggletare();
            }
            if (fstart == workstage){
                if (imrel & 0x80) {
                    if (otareout) tare = 0;
                    fstart = selflatch ; 
                    if (ototalrel8){
                        totalizing();
                        if (oricun && osptar && flag(ftare,test)) 
                           toggletare();
                    }
                }
            }
            if (fstart != workstage){
                
                if (snum || (keystatus != 5) && (keystatus != 6) && 
                        ((keystatus & restart) || ((keystatus & start) && 
                            ((imrel&relbit(relayZ) || oricun || otarein)))))
                {

                    if (otarein && wdisp && !(keystatus & restart)){
                        if (!snum){
                            tare = weight1 - zref;
                            atod = atodin();
                            weight(atod);
                        }
                        snum ++;
                        if (snum < 2) return;
                        snum = 0;
                    }

                    if (((keystatus & restart) && !tstart) || ((keystatus & start) && oricun)){
                        frestart = frestart3 = 1;
                        if(!ospord && osprev && !(imrel&0x80)) frestart2 = 1;                    
                    }
                    if ((keystatus & restart) || ((keystatus & start) && oricun)) tstart = 3;
                    t4flag  = 0;
                    t5flag  = 1;
                    ts4flag = 0;
                    if(leds) keystatus = 0;
                    if(!specrel) spret = spret & 0xff7f;
                    if(specrel){ 
                        spret = spret & 0xff5f;
                        speckey=0;
                    }
                    if(ospord && !osprev) {
                        spret = 0; 
                        spret |= relbit(byte(spdata[spind], 0)) ;
                    }
    
                    if(osptar) 
                        if(!oforcetar) 
                            if(flag(ftare,test)) 
                                  fstart = workstage ;
                    if(oforcetar) fstart = workstage ;
                    if(!osptar)   fstart = workstage;
                    if ((wdisp > zerodiv)) xbit(fpause0, off);
                }
                
                if(ospord && !osprev && (imrel & 0x80)) imrel = 0x80;
    
                if(specrel)
                    if ((keystatus & 0x20) && (imrel & 0x80) && (fstart)) 
                        speckey = 1 ;
                if (keystatus & stopkey) speckey = 0 ;
            }   
    
            if(ospord && !osprev) {
                if (!(spret & 0x40)){
                     s = spret & 0xfffe;
                     if ((spret & 0x01) && s) spret &= 0xfffe;
                }
            }
    
            switch(fstart){
                case stopstage : if (!ocheck) imrel = 0x00 ;
                                 xbit(fpause0, on);
                                 tdelay0 = dtime0 ; 
                                 break ;
                case selflatch : if (frestart) break;
                                 if(speckey == 0) imrel = 0x80;
                                 tdelay0 = dtime0 ;
                                 trestart = dtime0 + 10;   
                                 if(speckey == 1) imrel = 0xa0 ;
                                 break ;
            }
    
            if(specrel) if(speckey) imrel |= 0x20 ;
                    
            if (!(keystatus & start) && orel7on)
            {     // rel 7 on if op 24 on end not start and weight 0
                if (wdisp <= zerodiv) imrel |= 0x0040;
                else                  imrel &= 0xffbf;
            }
        
            if(ocheck && !fstart) imrel = 0; // rel dont work if checkweigher and not start
        
            if (!(imrel & 0x80) && !trestart && frestart){
                fstart = workstage ; frestart = 0;
            }
        
            if (orel7on && (imrel & 0x40)){ 
                if (imrel & 0x80) defpour();
                imrel &= 0xff7f;              // rel 8 dont work if rel 7 on
                if (specrel) imrel &= 0xffdf; // rel 6 dont work if rel 7 on
                speckey = 0;
                if (osprev && !ospord  && !dtime0){
                    spret = imrel; 
                    xbit(fpause0, on) ; 
                }
            }
        }

        if (ocheck && tdelay && !trel3on){
            imrel = 0;
        }else{
            if (ocheck && !trel3on && drel3on){
                trel3on = drel3on;
                drel3on = 0;
                oldimrel2 = oldimrel;
            }
            if (ocheck && trel3on){
                imrel = oldimrel2;
            }
        }

        if (imrel & 0x40){ 
            if (dtimez && !tdelayz) imrel &= 0xffbf;
        }else{
            tdelayz = dtimez;
        }
            
        f = 0;  // rel's dont light if all setpoin 0
        for (t = 0 ; t < spmax ; t++) if (spdata[t] > 0) f = 1;
        if ((!f && !l) && !ocheck) imrel = 0;
        if (xbit(fledoff, 2)) disled = -1;
        else{
            if(!ocheck){
                if (leds){
                    if (imrel & 0x80) fstart = selflatch ;
                    if (flag(fzero,test) && (fstart != workstage))
                        keystatus = 2;
                }
                disled = ~mirror(imrel);
            }
        }
/* 
//sprintf(ascbuf,"\n%x",(uint)imrel); comstring(ascbuf); while(fsend);
if (fstart)
{
    if (imrel == 0xff7f) imrel = 0x01;
    if (imrel == 0xff7e) imrel = 0x02;
    if (imrel == 0xff7c) imrel = 0x04;
    if (imrel == 0xff78) imrel = 0x08;
    if (imrel == 0xff70) imrel = 0x10;
    if (imrel == 0xff60) imrel = 0x20;
}    
*/
        if(tlife1 && ocheck){imrel = nlife1 ;sw1=0;} 
        ascbuf[0] = 0xfc ;
        ascbuf[1] = byte(imrel, 1) ;
        ascbuf[2] = checksum(ascbuf[0], ascbuf[1]) ;
        ascbuf[3] = 0xfd ;
        ascbuf[4] = byte(imrel, 0) ;
        ascbuf[5] = checksum(ascbuf[3], ascbuf[4]) ;
        if ((otrans || oprofi || onet) && obigdisp) return; // bid display in com 2
        if (comport) binstring(ascbuf , 6);
        else         xbinstring(ascbuf, 6);
    }
    getkey() ;
}
void    getkey()
{
    uchar   ret, res ;
    tneway = 6 ;
    bytest = 0 ;
    while(tneway)
    {
        res = 0 ;
        if (comport)
        {
            if (comstat())
            {
                ret = getcom();
                res = 1;
            }
        }
        else
        {
            if (xcomstat())
            {
                ret = xgetcom();
                res=1;
                tledoff=onesec;
                xbit(fledoff, 0);
            }
            else if (!leds && !tledoff) xbit(fledoff, 1);
        }
        if (res)
        {
            switch(bytest)
            {
                case 0: if (((ret&0xfc) == 0xfc)&&
                                ((ret&0x03) == 0x00)) bytest = 1 ; // from relsys 0
                    break ;
                case 1: keystatus = ret ;
                    bytest = 2 ;
                    break ;
                case 2: ret += keystatus + 0xfc ;
                    if (ret)  keystatus = 0 ;
                    tneway = bytest = 0 ;
                }
        }
    }
}
uchar   checksum(uchar ret1, uchar ret2)
{
    return(~(ret1+ret2) + 1) ;
}
