#include "tiger.h"
/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³               Main process (resident loop)               ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/
void    option_change();
void    transmit_weight();
void    key_press(uchar ret);
void    lock_key();

void    battery(void)
{
    uchar t ;

    if (is_battery != 1955){
        is_battery  = 1955  ;               // battery's signature
        blink(" BAT  ", 30) ;
        dtime  = dzone  = 0 ;               // dead time and zone
        dtime0 = dtime8 = rel8off= 0 ;      // delay in zero and self-latching point
        kunit  = spmax  = 0 ;               // unit weight, setpoint's max
        acode[0] = tare = 0 ;               // code and tare
        order  = total  = 0 ;               // subtotal
        gorder = gtotal = 0 ;               // grandtotal
        ranlo  = ranhi  = 0 ;               // ranges for auto totals
        target = tollo  = tolhi = 0 ;       // target and tollerances
        for (t = 0; t < 4    ; t++) sptime1[t] = sptime[t] = 0 ;            
        for (t = 0; t < spnum; t++) spdata [t] = 0 ; // setpoints            
        for (t = 0; t < splim; t++) rel3on [t] = 0;
        meof   = &mcode ;                   // pointer to mcode 'eof'
        zerodiv  = zeroneg = 0 ;            // zero range for relayZ(ero)
//        greendiv = cw1div ;                 // green range for checkweigher
        dpuls = dpulsl = flock = dtimez = 0;
        qtbl  = 0.5;
        qtemp = qtbl * 6;
    }
    else
        if (ozmem){                   // Get zero from memory
            if ((!ocheck)&(!odeny))
                if (!xbit(fpour, test))
                if (spmax > 0) fsetpoint(0) ;
            znext = zref = lastzref ;
        }
}
void    process(void)
{
    static uint  simul;
    uchar ret, t[3];
    uint  wdiffer, fstam, ind;

    ret = keyboard() ;
    blight();
    if (ret){
        if (!flock || (ret == '2')) key_press(ret);
        else if (ret == 'F') lock_key();
        if (oautof){
            qtoff = 0 ;
            toff = t24sec ;           // auto turn off counters reset
        }
    }
    if (!voltage() &&(!lboff)) switchoff() ;       // shut off by low voltage
    if (testbit(fsamp)){
        atod = atodin();
        weight(atod);
        blight();
        if (fcount) flag(fpcs,  on) ; else
            if (okilo)  flag(fkilo, on) ; else
                if (opound) flag(fpound,on) ;
        flag(ftare, tare>0) ;
        if (hopref) drefresh(twosec) ;
        if ((!fover)&&(!tputs))
        {                    // the weight show
            if (odisp) display(wdisp) ; else       // dinamic display
                if (fstab) display(wdisp) ;            // stable display
            if(oerr10)if(wdisp==0)if(((zref0>>8)+2000<(zref>>8)) || ((zref0>>8)-2000>(zref>>8))) errors(10, 0);
        }
        if (weight2 < wdisp)
        {                     // weight's change direction
            wdiffer = wdisp - weight2 ;
            fdir =  1 ;
            weight2 = wdisp ;
        }
        else
            if (weight2 > wdisp)
            {
                wdiffer = weight2 - wdisp ;
                fdir = -1 ;
                weight2 = wdisp ;
            }
            else
            {
                wdiffer = 0 ;
                fdir =  0 ;
            }
        if(ppres && fstab) ppres=0;
        if (oautof)
        {
            if (wdiffer > cw2div)
            {
                qtoff = 0 ;
                toff = t24sec ;        // auto turn off counters reset
            }
            else
            {
                if (!toff)
                    if (++qtoff > 9) INT1 = 0;      // auto turn off after 4 min
                    else toff = t24sec ;            //      without operation
            }
        }
        if(rel8off!=0)
        {
            loops=rel8off;
            if((fstart&0x02)!=0x02)
            {
                qtrel8of=0;
                trel8of=1;
            }
            else
            {
                if(!trel8of)
                    if (++qtrel8of > loops)
                    {
                        keystatus = 0x04 ;
                        imrel=0;      //  turn off relay 8
                        relcom(imrel, 0) ;
                    }
                    else trel8of = 1 ;            //      without operation
            }
        }
        // bcd communication
        fstam=0;
        if (wdisp >(long)wstop) wdisp = wstop ;
        if (bcd420)
        {                     // 4-20mA format
            if ((long)myw>(long)wstop){myw = bcdini ? 0 :-1 ;fstam=1;}
                if ((long)myw>=(long)(zeroneg) && (long)myw<(long)0){imrel1=-1;fstam=1;}
                else
                {
                    if (myw<zeroneg) {imrel1 = bcdini ? 0 :-1 ;fstam=1;}
                        else imrel1 = 0x0fff*myw/wstop ;
                }
        }
        else
        {
            if (bcdout)
            {                      // binary format
                if (bcdsel)
                {
                    imrel1 = 0x7fff*wdisp/wstop ;
                }
                else
                {
                    if (fover|fsign) imrel1 = bcdini ? 0 :-1 ;
                    else imrel1 = 0xffff*wdisp/wstop ;
                }
            }
            else
            {                             // bcd format
                imrel1 = dec4bcd(wdisp) ;
            }
        }
        if (bcdpol)
            if ((bcdsel)||(!fover) && !fstam)
                imrel1 = ~imrel1 ;
        /***********************************************/
        if (!odeny){
            if (ocheck) checkweigher(wdisp) ;
            else
                if (spmax > 0){
                    if (!xbit(fpour,2)){
                        fsetpoint(wdisp) ; 
                    }
                    else if (flag(ftare, 2)) psetpoint(wdisp) ;
                }
        }
        if ((fdelay) && (!tdelay) && (!tdelay0)){
            fdelay = 0 ;
            imrel = spret ;     
        }
        if ((flife)&&(!tlife)){       // relay's longlife time is over
            imrel ^= relbit(nlife) ; flife = 0 ;   // only for checkweigher option
        }

        if(dtime>hafsec  && !tdelay && ocheck){
            if(imrel&1){nlife1=1 ;ind=0;sw1=0;}
            if(imrel&2){nlife1=2 ;ind=1;sw1=0;}
            if(imrel&4){nlife1=4 ;ind=2;sw1=0;}
            if(imrel&nlife1 && sw1==0){sw1=1;tlife1 =(uint)sptime1[ind] + tdelay;}
        }

        watchdog();
        relcom(imrel, 0) ;        // new relbox, routine in tigerc.c
        if(bcd) relset(imrel1) ;            // old relbox
        if ((oconti) && (otrans))           // continuous transmit to PC
            if (!fsend)  wttrans() ;
        if (!xbit(fnewwg, test))
        {                 // new printed weight check
            if (wdisp < round) xbit(fnewwg, on) ;
            else if (ophalf)
            {
                if (wdisp < wprint/2) xbit(fnewwg, on) ;
            }
            else if (wdisp != wprint) xbit(fnewwg, on) ;
        }
        
        if (oSendWeight && otrans && fstab &&xbit(fnewwg, test)){
            if ((wdisp>0) && (wdisp>=ranlo))
            {
//                if (ranhi && (wdisp > ranhi)) goto tail ;
                beep(1) ;
                wttrans() ;
                totalizing() ; xbit(fnewwg, off) ;
            }
        }

        if (fautom && !oconti && oautom)           // automatic totalizing
            if (fstab || onostab)                  // if (tstab < twosec-3)
            if (xbit(fnewwg, test))
            if ((wdisp>0)&&(wdisp>=ranlo))
            {
                if ((ranhi)&&(wdisp>ranhi)) goto tail ;
                beep(1) ;
                puts("\1 STORE") ;         // weight storage
                totalizing() ; xbit(fnewwg, off) ;
                pause(hafsec);
                tail: ;
            }

    }
    transmit_weight();
    stackok();
}

uint    dec4bcd(long dec)
{
    uchar   s[10] ;
    uint    ret ;

    sprintf(ascbuf, "%06ld", labs(dec)) ;
    strncpy(s, ascbuf+bcdisp-1, 4) ; s[4] = 0 ;
    ret  = ((uint)(s[0]-'0')) << 12 ;       // #2
    ret += ((uint)(s[1]-'0')) <<  8 ;       // #3
    ret += ((uint)(s[2]-'0')) <<  4 ;       // #4
    ret += ((uint)(s[3]-'0')) ;             // #5
    if (bcdsel)
    {
        if (ret > 0x7999) ret = 0x7999 ;
        if (fsign)  ret |= 0x8000 ;
        else    if (fover)
        {
            ret = bcdini ? 0 :-1 ;
            if (bcdpol) ret = ~ret ;
        }
    }
    else
    {
        if (fover|fsign)  ret = bcdini ? 0 :-1 ;
    }
    return (ret) ;
}

bit     voltage()
{
    if (!getbitport(6))
    {                   // voltage is low
        if (tlobat == 0)
        {
            tlobat = hafsec ;
            flag(flo, flag(flo, test)^1) ;
            if (flag(flo, test)) lboff-- ;
        }
        flag(fbat, on) ;
        return false ;
    }
    else
    {
        lboff = twomin ;
        flag(flo, off) ; flag(fbat, off) ;
        return true ;
    }
}

void    option_change()
{
    uint  i,j,c[70],l=1;
    uchar ch,cn,c1,c2,cb[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0};
    if (l) ungetch(); l=0;
    for(i=0;i<70;i++) c[i]=0;
    if (memcmp(options, opdef+(opset-1)*8, 8)){
        for(i=0;i<8;i++){
            if (options[i] != opdef[((opset-1)*8)+i]){
                ch = options[i] ^ opdef[((opset-1)*8)+i];
                for(j=0;j<8;j++){
                    cn = ch & cb[j];
                    c2 = opdef[((opset-1)*8)+i] & cb[j];
                    if (cn>0){
                        if (c2) c[(i*8)+j]=1;
                        else    c[(i*8)+j]=2;
                    }
                    cn=0;
                }
            }
        }
    }
    else{
        printf("\nS.OP-%02d",(uint)opset);getch();
        pause(10); puts("\n M0M  "); pause(10);
    }
    printf("\nS.OP-%02d",(uint)opset);
    getch();
    for(i=0;i<70;i++) if(c[i]) c1=i;
    for(i=0;i<70;i++){
        if (c[i]){
            do{
                watchdog();
                switch(keypress()){
                    case 'T': cn=0;
                            if(cdouble('T'))if(cdouble('T')){
                                cn=1;
                                if (c[i]==2) printf("\nOP -%02d",(uint)i+1);
                                else         printf("\nOP  %02d",(uint)i+1);
                            }
                            else return;
                            break;
                    case 'F': cn=0;
                            if(getch()=='F'){
                                cn=1;
                                if (c[i]==2) printf("\nOP -%02d",(uint)i+1);
                                else         printf("\nOP  %02d",(uint)i+1);
                            }
                            else return;
                            break;
                }
                if (cn) break;
            }
            while(1);
            if(c1 == i){
                getch(); t2=5; 
                while(t2){
                    watchdog();
                }
            }
        }
    }
}

void blight(void)
{
    if((fdir!=0  || swkeyb) && qtbl!=0){backlight(1);qtbacklight=0;tbacklight=onesec;swkeyb=0;}
        else
    {
        if(!tbacklight || qtbl==99.0)
        {
            if (++qtbacklight >qtemp){backlight(0);}      //  turn off back light after 4 min
            else tbacklight = onesec ;
        }
    }
}


void transmit_weight()
{
    uchar ret;

    if (oprofi){
        if (otrans){
            ret = 0 ;
            if (comport){ if (xcomstat()) ret = xgetcom(); }
            else        { if (comstat ()) ret = getcom() ; }
            switch(ret){
//                case 'J' : sendprofiID()     ; break ; // profibus ID
//                case 'A' : sendaddress()     ; break ; // profibus address
                case 'W' : sendweight()      ; break ; // weight
                case 'I' : sendidentifier()  ; break ; // IDENTIFIRE
                case 'R' : sendatod()        ; break ; // ATOD
                case 'F' : sendfullscale()   ; break ; // FULL SCALE
                case 'V' : sendtarevale()    ; break ; // VALUE OF TARE
                case 'D' : senddecimalpoint(); break ; // DECIMAL POINT
                case 'Z' : sendzero()        ;         // zero
                           treqz = twosec    ;
                           ftranzero = transzero = 1; break ;
                case 'T' : sendtare()        ;         // tare
                           treqt = twosec    ;
                           ftranstare = transtare = 1; break ;
                case 'S' : resersacle()      ; break ; // RESET
            }
            if ((!treqt)&&(transtare)){
                
                if (!oprofi){
                    switch(transtare){
                        case 2 : if (comport) xcomstring(ack) ; else comstring(ack) ;
                                 break ;
                        case 1 : if (comport) xcomstring(nak) ; else comstring(nak) ;
                                 break ;
                    }
                }
                transtare = 0 ;
            }
            if ((!treqz)&&(transzero)){
                if (!oprofi){
                    switch(transzero){
                        case 2 : if (comport) xcomstring(ack) ; else comstring(ack) ;
                                 break ;
                        case 1 : if (comport) xcomstring(nak) ; else comstring(nak) ;
                                 break ;
                    }
                }
                transzero = 0 ;
            }
        }        
    }else{
        if (!onet){
            if (otrans){
                ret = 0 ;
                if (comport){ if (xcomstat()) ret = xgetcom(); }
                else { 
                    if (comstat ()){
                        ret = getcom (); 
                        ftranzero = ftranstare = 1;
                    }
                }
                switch(ret){
                    case 'w' : 
                    case 'W' : if (odynam)  wttrans() ;   // weight
                               else  tdelay2 = twosec ; break ;
                    case 't' : ttrans(); break;
                    case 'T' : treqt = twosec ;           // tare
                               transtare =  1 ;         break ;
                    case 'Z' : treqz = twosec ;           // zero
                               transzero =  1 ;         break ;
                    case 'S' : 
                    case 'N' :
                    case 'C' : TotalTrans(ret)  ; break;
                }
                if ((tdelay2)&&(tstab<onesec)){
                     tdelay2 = 0 ;
                     wttrans() ;
                }
                if ((!treqt)&&(transtare)){
                    switch(transtare){
                        case 2 : if (comport) xcomstring(ack) ; else comstring(ack) ;
                                 break ;
                        case 1 : if (comport) xcomstring(nak) ; else comstring(nak) ;
                                  break ;
                    }
                transtare = 0 ;
                }
                if ((!treqz)&&(transzero)){
                    switch(transzero){
                        case 2 : if (comport) xcomstring(ack) ; else comstring(ack) ;
                                 break ;
                        case 1 : if (comport) xcomstring(nak) ; else comstring(nak) ;
                                 break ;
                    }
                    transzero = 0 ;
                }
            }
        }
        else{            
            /* ----- communication branch for net----- */
            if (onet){
                switch(comport) {
                    case 0 : if ( comstat()) network( getcom()) ;  break ;
                    case 1 : if (xcomstat()) network(xgetcom()) ;  break ;
                }
                if (phext && (!trsnet)) {
                    phext = await = 0 ;
                }    
                if ((transtare) && (!treqt)) {
                    switch(transtare) {
                        case 2 : answer(ack) ; break ;
                        case 1 : answer(nak) ; break ;
                    }
                    transtare = 0 ;
                }
                if ((transzero) && (!treqz)) {
                    switch(transzero) {
                        case 2 : answer(ack) ; break ;
                        case 1 : answer(nak) ; break ;
                    }
                    transzero = 0 ;
                }
            }
        }
    }
}

void xputs(uchar *s)
{
    uchar   status=0, str[15], f;
    //float   fdisp ;

    if(flag(fzero ,test)) status |= 0x01;
    if(flag(ftare ,test)) status |= 0x02;
    if(flag(fkilo ,test)) status |= 0x04;
    if(flag(fpound,test)) status |= 0x08;
    if(flag(fbat  ,test)) status |= 0x20;
    if(flag(fpcs  ,test)) status |= 0x40;

    if (otrans || oprofi || onet) f = 0;
    else                          f = 1;

    if (s[0] == '#'){
        if (!obigdisp) return;    
        fdisp = (float)wdisp/fpow(10, decim) ;
        if(flag(fpcs  ,test) || !decim) 
            sprintf(secbuf, "\n%c%6ld",status,wdisp) ;        
        else
            sprintf(secbuf, "\n%c%7.*f",status, (uint)decim, fdisp) ;
        if (f) comstring(secbuf);
        else  xcomstring(secbuf);
    }else{
        puts(s);
        if (!obigdisp) return;
        sprintf(secbuf, "\n%c%s",status,s) ;
        if (f) comstring(secbuf);
        else  xcomstring(secbuf);
    }
}

void key_press(uchar ret)
{
    ulong val;
    uchar fun;

    switch(ret){
        case '0': toggle0() ;       break ;
        case '1': if (!ototalrel8) toggle1() ; break ;
        case '2': getcode() ; break ;
        case '3': if (acode[0] == 0 || acode[0] == '0') break;
                  if (odeny)   break ;
                  if (ocheck) define_cw() ;
                  else        define_sp(); break ;
        case '4': subdisp() ;       break ;
        case '5': csample() ;       break ;
        case '6': unitwgt() ;       break ;
        case '7': val = tare;
                  manualtare(0);
                  OneTare = tare;
                  flag(fmidl,1); flag(fmidr,1);
                  tuint = getfvalue(" NUM  ", 0, 0, 0) ;
                  flag(fmidl,0); flag(fmidr,0);
                  tare = (tuint * tare) + val;
                  break ;
        case '8': brutwgt() ;       break ;
        case '9': clear_tot(0);     break ;
        case 'F': clrscr() ;
                  flag(fmidl, 1) ;  flag(fmidr, 1) ;
                  xbit(fpuse, 1) ;
                  printf("\nFUN-  ") ;  fun = 0 ;
                  texitfun = exitfun;  
                  switch(fun = getvalue(fun, 99, 5, 2)){
                      case -1:  if (oconti)                 break  ;
                                if (otrans)                 break  ;
                      default:
                      case  0:                                  break  ;
                      case  1:  if (clockin("d", 0)) datime() ; break  ;
                      case  2:  ranges() ;                      break  ;
                      case  3:  if (!ototalrel8) ranset() ;     break  ;
                      case  5:  dtimez = cdelay("DELAY.Z", dtimez, 255); break ;
                      case  7:  delete() ;                      break  ;
                      case  8:  dtime8 = cdelay("8 ON  ", dtime8, 255); break ;
                      case  9:  smcode() ;                              break ;
                      case 10:  dtime0 = cdelay("DELAY0", dtime0, 255); break ;
                      case 11:  splong() ;                              break ;
                      case 12: _dload() ;                               break ;
                      case 13:  dtime  = cdelay("DELAY ", dtime, 255) ; break ;
                      case 14:  manual_sp() ;                           break ;
                      case 15:  manual_bcd() ;                          break ;

                      case 17: relontimers(); break;
                      case 18: splong1() ;    break;
                      case 69: lock_key();    break;
                      case 80: wfactor = factor*fpow(10, 3)*16384.0/wfull ;
                               printf("\n%7.6f", wfactor);getch();
                               break;
                      case 81: option_change(); break  ;
                      case 82: clrscr();
                               flag(fmidl, 1); flag(fmidr, 1);
                               printf("\n BRUTE"); pause(onesec);
                               if (fstab) wdisp = weight1 - zref0;
                               else wdisp = weight0 - zref0;
                               display(factorize(wdisp)) ;
                               if (okilo)  flag(fkilo, fstab);
                               if (opound) flag(fpound, fstab);
                               getch();
                               flag(fmidl, 0) ; flag(fmidr, 0) ;
                               stackok(); break;
                      case 83: while(!keyboard()){
                                   atod = atodin();
                                   printf("\1 %5u", atod);
                               }break;
                      case 84: while(1);
                      case 85: fun85();  break;
                      case 86: fun86(); break;
                      case 87: qtbl = getfvalue("T.BL  ", qtbl, 1, 999) ;
                               qtemp=qtbl*6;  pdone();   break;
                      case 88: membyte() ;               break  ;
                      case 89: sprintf(ascbuf,"\n%-6ld",sn);
                               phase = label("S . N   ", ascbuf) ;
                               break;
                      case 90: sprintf(ascbuf,"\n%6d",(uint)ident);
                               clrscr() ;
                               phase = label(" IDENT", ascbuf) ;
                               break;
                      case 91: fun88();      break;
                      case 98: clear_tot(1); break;
                      case 99: clear_all() ; break;
                  }
                  xbit(fpuse, 0) ;
                  flag(fmidl, 0) ; flag(fmidr, 0) ;
                  break ;
        case 'P': if (oconti)     break ;   // no print by cont.transmit
                  if (ototalrel8) break;    
                    //    to computer
                  if (otrans){
                      if ((wdisp)&&(xbit(fnewwg, 0))) totalizing() ;
                      wttrans() ; break ;   // weight transmission
                  }
                  timeout = hafsec ;
                  ret = false ;
                  while(timeout){
                      watchdog() ;
                      if (keyboard() == 'P'){
                          ret = true ;
                          break ;
                      }
                  }
                  clrscr(); break ;
    }
}

void lock_key()
{
    uchar f, n=0;

    timeout = 30;
    if (!flock) puts("\n LOCK ");
    if (flock)  puts("\nULOCK ");
    while (timeout){
        f = keyboard();
        if (f){
            if (f == '9' && !n)          n++;
            else if (f == '6' && n == 1) n++;
                 else                    n=0;
        }
        watchdog();
        if (n == 2){
            pdone();
            if (flock == 1) flock = 0;
            else            flock = 1;
            timeout = 0;
        }
    }
}

