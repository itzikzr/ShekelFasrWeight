#include "tiger.h"
/*
     ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
     ³  Test mode : calibration & definition of main parameters ³Û
     ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

void fun0();     // Test mode initializator
void fun1();     // Basic test mode (weight display)
void fun2();     // Basic test mode (A/D display)
void fun3();     // Identifier display
void fun4();     // Password to special parameters
void fun5();     // Calibration: data entry and trimmer setting
void fun6();     // Calibration: zero sampling
void fun7();     // Calibration: reference load sampling and save
void fun8();     // Saving data in NOVRAM
void fun9();     // Option programming
void fun11();    // Internal UART RS232 parameters setting
void fun12();    // External UART RS232 parameters setting
void fun13();    // Print format parameters
void fun14();    // Bcd display part
void fun15();    // Hidden options programming
void fun16();    // Set trimmer directly
void fun17();    // Set target atod
void fun18();    // New zref after fun17()
void fun19();    // serial number
void fun20();    // Waiting for confirmation before saving
void fun21();
void nextop();   // Displaying next option
void shiftop();  // Shift rightmost byte of option number
void nexthop();  // Displaying next hidden option
void opsetch();  // Opset matching to opdef[]
uchar eight();   // 888888

code  void *testfun[] =         // Test functions branch table
{
    fun0, fun1 , fun2 , fun3 , fun4 , fun5 , fun6 , fun7 , fun8, fun9, nada,
    fun11, fun12, fun13, fun14, fun15, fun16, fun17, fun18, fun19, fun20,fun21
} ;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void    testmode(void)
{
    if (kbhit())
    {
        backlight(1);
        swkeyb=1;
        switch (getch())
        {
            case 'T': testkeyt(); break;
            case 'Z': testkeyz(); break;
            case 'F': ffkey = 1;
                      if (phase==1) phase=3; 
                      else {fblink=1; phase++;}
                      if (phase < 5) testkeyz();
                      else{phase--; fblink = 1; testkeyt(); }
                      break;  
            case 'P': testkeyp(); break;
            default : pass = keylist[scancode];
                if (isdigit(pass)) testkeyd(pass-'0');
                pass = 0XC0;
            }
    }
    if (testbit(fsamp))
    {
        atod = atodin();
        if ((phase<9)||(phase==18))
        {
            weight(atod) ;
        }
        //backlight(0);
        execute(testfun[phase]);
    }
    stackok();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void  fun0()                   // Test mode startup phase
{
    if (testbit(fstab))
    {
        atod = atodin();
        word(weight0, 1) = atod;
        fsamp = 0;
        comini(0X65);
        xcomini(0X65);
    }

    oldtare=tare;
    zref = weight1 = weight0;
    tare =weight1-zref ;
    hometest();
}

void    fun1()                // Basic test mode (weighing)
{
    if (tblink)
    {
        if (!fblink) display(wdisp);
    }
    else
    {
        fblink = ~fblink;
        if (fblink)
        {
            puts("\n TEST ");
            tblink = hafsec;
        }
        else tblink = twosec;
    }
}

void    fun2()                // Basic test mode (A/D)
{
    if (tblink)
    {
        if (!fblink) printf("\1 %5u", atod);
    }
    else
    {
        fblink = ~fblink;
        if (fblink)
        {
            puts("\n  A-D ");
            tblink = hafsec;
        }
        else tblink = twosec;
    }
}

void fun3()                   // Identifier display
{
    clrscr() ;
    xbit(ftuse, 1) ;
    sprintf(ascbuf,"\n%6d",(uint)ident);
    phase = label(" IDENT", ascbuf) ;
    if (phase != 'T') phase = eight() ;
    if (phase != 'T')
    {
        opsetch() ;                         xbit(ffuse, 1) ;
        phase = getvalue(opset, 51, 5, 2) ; xbit(ffuse, 0) ;

        if (phase)
        {
            if (memcmp(&null, opdef+(phase-1)*8, 8))
            {
                opset  = phase ;
                memcpy(options, opdef+(opset-1)*8, 8) ;
                /*************tanks*******************/
                if( ((opset>40) && (opset<47)) || ((opset>22) && (opset<25)) || (opset == 7)){
                    hidop[0]&=0xe;
                }
                else{
                        hidop[0]|=1;
                }
                /*******************************/
            }
        }
        if (returncode)
        {
            if (options[0]&1) puts("\nOP -01");
            else puts("\nOP  01");
            fblink = 0;
            pass  = 0XC0;
            phase = 9;
        }
        else
        {
            fblink = 1 ;
            phase = 20 ;
        }
    }
    else
    {
        if (cdouble('T'))
        {
            ungetch() ; phase = 3 ;
        }
        else hometest() ;
    }
}

void fun4()                   // Password to special parameters programming
{
    clrscr() ;
    puts("\n PASS ");
    timeout = twosec;
    pass = 4;
    while (timeout)
    {
        if (password()) phase = 15;
        watchdog();
    }
    if (phase==15)
    {
        if (hidop[0]&1) puts("\nHOP-01");
        else            puts("\nHOP 01");
        fblink = 0;
        pass = 0XC0;
    }
    else hometest();
}

xdata uchar ret, sw_yes_no=1,sw_con=0;
xdata float mwfactor;
void fun5()                   // First calibration phase
{
    code  uchar *nam[] = { " SET  ", "CHECK " } ;
    code  uchar *yes[] = { "   YES", "   NO " } ;

    xbit(fzenb, 1) ; xbit(fdoff, 1) ;       // decimal point reset
    decperiod = decim ;
    wprint = getfvalue("\n FULL ", wfull, decim,0);
    xbit(fzenb, 0) ; xbit(fdoff, 0) ;
    if (!returncode) recycle() ;
    if (!wprint)
    {
        dif = decim - decperiod ;
        if (!dif)
        {
            mwfactor = factor*16384000.0/wfull ;
            wfactor = getfvalue("FACTOR",mwfactor, 9,0);
            if (!returncode) recycle() ;
            else
            {
                if(wfactor!=mwfactor)
                    while(!sw_con)
                {
                    watchdog();
                    ret = label(nam[(uint)0], yes[(uint)sw_yes_no]) ;
                    switch(ret)
                    {
                        case 'Z' : sw_yes_no^=1;
                            break;
                        case 'T' : if (cdouble('T'))
                                if(!sw_yes_no)
                            {
                                sw_con=1;
                                break ;
                            }
                            else
                            {
                                wfactor=mwfactor;
                                sw_con=1;
                                break;
                            }
                            else recycle();
                            case 'F' :
                            if(!sw_yes_no)
                            {
                                sw_con=1;
                                break ;
                            }
                            else
                            {
                                wfactor=mwfactor;
                                sw_con=1;
                                break;
                            }
                        }
                }
            }
            rfactor = wfactor*wfull/16384000.0 ;
            wprint  = getfvalue("CON.GRO", wprint, decperiod,0) ;
            if ((wprint)&&(returncode))
            {
                atod = atodin() ; weight(atod) ;
                zref = weight0 - (float)wprint/rfactor ;
                lastzref = zref ;
            }
            factor = rfactor;
        }
        else
        {
            decim = decperiod ;
            rfactor = fpow(10, dif) ;
            wfull /= rfactor ;
            wstop /= rfactor ;
            factor /= rfactor;
            rfactor = factor ;
        }
        rfactor /= round;
        setdiv();
        fblink = 1;
        phase = 8 ;
        return ;
    }
    wfull  = wprint ;
    wtrack = wprint * 2/3 ;
    wtrack = lloor(wtrack) ;
    wtrack = getfvalue("\n LOAD ", wtrack, decperiod,0);
    if (!returncode) recycle() ;
    if (!wtrack)     recycle() ;
    wprint = (hiatod) ? hiatod : offlim ;
    wprint = getfvalue("\n A-D0 ", wprint, 0,0) ;
    if (!returncode) recycle() ;
    if (!wprint)     recycle() ;
    hiatod = wprint ;
    phase = trimfix(hiatod) ;
    if (phase)
    {
        trimval = phase;
        puts("\n======");
        filcof = ramp = 0;
        fstab = 0;
        tfil = filrate;
        tgostab = stabin;
        rfactor = factor;
        memcpy(novdata, novdef, 6);
        phase = 6;
    }
    else
    {
        trimset(trimval);
        hometest();
    }
}

void fun6()                   // Calibration: zero sampling
{
    if (!fstab) timeout = twosec;
    if (!timeout)
    {
        zref = weight0;
        sformat(icbuf, wtrack, decperiod);
        label("\n  PUT ", icbuf);
        ungetch() ;
        returncode = true ;
        switch(getch())
        {
            case 'T': if (!cdouble('T')) returncode = false ;

            case 'F': if (returncode)
            {
                puts("\n  CAL ");
                filcof = ramp = 0;
                fstab = 0;
                tfil = filrate;
                tgostab = stabin;
                phase = 7;
                break ;
            }

            default : hometest() ;
        }
    }
}

void fun7()                   // Calibration: reference load sampling
{
    if (!fstab) timeout = twosec;
    if (!timeout)
    {
        if (weight0<zref) recycle();
        rfactor = (float) wtrack;
        rfactor /= (weight0-zref);
        factor = rfactor;
        rfactor /= round;
        setdiv();
        wfactor = factor*fpow(10, 3)*16384.0/wfull ;
        printf("\n%7.6f", wfactor);
        timeout = twosec;
        while ((timeout)&&(!kbhit())) watchdog();
        if (kbhit())
        {
            getch() ; hometest();
        }
        lastzref = zref0 = zref ;      // for farther usage by op.7
        decim  = decperiod ;
        phase  = 8 ;
        fblink = 1 ;
    }
}

void fun8()                     // Saving data in NOVRAM
{
    if (testbit(fblink))
    {
        puts("\n DONE ") ;
        if (novsave(0)) timeout = onesec;
        else errors(3, 0);
    }
    if (!timeout) hometest();
}

void fun9()                   // Options programming
{
    if (!pass)
    {
        shiftop();
        pass = 0XC0;
        fblink = 0;
    }
    else if (!treqz)
    {
        if (keypress()=='Z')
        {
            fblink = 0;
            treqz = 5;
            nextop();
        }
        if ((testbit(fblink))&&(undisp(5)))
        {
            phase = undisp(5) - 1;
            options[phase>>3] |= 1<<(phase&7);
            if (unput(4)==' ') puts("\4-");
            else
            {
                puts("\4 ");
                options[phase>>3] ^= 1<<(phase&7);
            }
        }
    }
    phase = 9;
}

void fun11()                  // Internal UART RS232 parameters setting
{
    if (!pass)
    {
        pass = 0XC0;
        if ((com1&7)==7) com1 &= 0XF8;
        else com1++;
        printf("\n%s.", ubaudisp[com1&7]);
    }
    else
    {
        if (keypress()=='Z')
        {
            if ((!timeout)&&(!treqz))
            {
                if ((com1&0X30)==0X30) com1 ^= 0X60;
                else com1 += 0X10;
                printf("\4%s", uformat[(com1>>4)&7]);
                treqz = hafsec;
            }
        }
        else timeout = 2;
    }
}

void fun12()                  // External UART RS232 parameters setting
{
    if (!pass)
    {
        pass = 0XC0;
        if ((com2&7)==7) com2 &= 0XF8;
        else com2++;
        printf("\n%s.", xbaudisp[com2&7]);
    }
    else
    {
        if (keypress()=='Z')
        {
            if ((!timeout)&&(!treqz))
            {
                if ((com2&0X30)==0X30) com2 ^= 0X60;
                else com2 += 0X10;
                printf("\4%s", uformat[(com2>>4)&7]);
                treqz = hafsec;
            }
        }
        else timeout = 2;
    }
}

void fun13()
{
    tblink = fblink = 0 ;

    if (returncode)
    {
        nforce = getfvalue(" FORCE", nforce, decim,0) ;
        wforce = nforce/factor ;
        if ((nforce)&&(returncode))
        {
            wtrack = getfvalue("TFORCE", dforce, 1,0) ;
            if (wtrack > fullsec)  dforce = fullsec ; else
                if (wtrack < decsec )  dforce = tensec  ; else
                dforce = (uchar) wtrack ;
        }
        else  dforce = 0 ;
    }
    fblink = 1 ;
    if (returncode)
        if (bcd)
        if ((!bcd420)&&(!bcdout)) phase = 14 ;
    else phase = 19 ;
    else phase = 14 ;
    else phase = 19 ;
}

void fun14()
{    
    code  uchar *nam[] = { " SET  ", "CHECK " } ;
    code  uchar *yes[] = { "   YES", "   NO " } ;

    if (bcd) {
        if (testbit(fblink))
        {
            puts("\nBCD.   ");
            if ((!bcdisp)||(bcdisp>3)) bcdisp = 3 ;
        }
        cursor = 4 ;
        switch(bcdisp)
        {
            case 1 : puts("1-4") ; break ;
            case 2 : puts("2-5") ; break ;
            case 3 : puts("3-6") ; break ;
        }
    }
    else phase = 19 ;    
}

void fun15()                  // Hidden options programming
{
    if (!pass)
    {
        nexthop();
        pass = 0XC0;
        fblink = 0;
    }
    else if (!treqz)
    {
        if (keypress()=='Z')
        {
            fblink = 0;
            treqz = 8;
            nexthop();
        }
        if ((testbit(fblink))&&(undisp(5)))
        {
            phase = undisp(5) - 1;
            hidop[phase>>3] |= 1<<(phase&7);
            if (unput(4)==' ') puts("\4-");
            else
            {
                puts("\4 ");
                hidop[phase>>3] ^= 1<<(phase&7);
            }
        }
    }
    phase = 15;
}

void fun16()
{
    fblink = 1 ;
    tblink = 0 ;
    wprint = getfvalue(" TRIM ", (ulong)trimval, 0,0) ;
    if (returncode)
    {
        trimval = (uchar)wprint ;
        puts("\n ATOD ") ;
        trimset(trimval) ;
        printf("\n%6d", atodin()) ; pause(twosec) ;
        fblink = 1 ;
        phase = 8 ;
    }
    else hometest() ;
}

void fun17()
{
    fblink = tblink = 0 ;
    wprint = getfvalue(" ATOD ", (ulong)hiatod, 0,0) ;
    if (returncode)
    {
        hiatod  = (uint)wprint ;
        trimval = trimfix(hiatod) ;
        printf("\n TRIM ") ; pause(hafsec) ;
        printf("\n%6d", (uint)trimval) ;
        pause(twosec) ;
        puts("\n======") ;
        filcof = ramp = 0;
        fstab = 0;
        tfil = filrate;
        tgostab = stabin;
        phase = 18 ;
    }
    else hometest() ;
}
void fun18()
{
    if (!fstab) timeout = twosec ;
    if (!timeout)
    {
        lastzref = zref0 = zref = weight0 ;
        fblink  = 1 ;
        phase = 8 ;
    }
}

uchar ret19;
void fun19()
{
    
    code  uchar *nam[]  = { " SET  ", "CHECK " } ;
    code  uchar *yes[]  = { "   YES", "   NO " } ;
    code  uchar *nam1[] = { "    KEY 3 ON    "} ;
    code  uchar *yes1[] = { "      YES       ", "       NO       " } ;
    code  uchar *nam2[] = { "  3 ù÷î øåùôéà  "} ;
    code  uchar *yes2[] = { "       ïë       ", "       àì       " } ;

    if (!odeny && !ocheck){
        tblink = fblink = 0 ;
        target = (ulong)setplim;
        setplim = (uchar)getfvalue("S.P - M", (ulong)setplim, 0, 16);
        if (setplim == 0) setplim = target;
        if (returncode){ 
            fblink = 1; phase = 20 ;
        }
        if(!returncode){ 
            ungetch(); fblink = 1; phase = 20 ; 
        }
    }
    target = 0;

    ret19 = 1;
    while(ret19){
        ret19 = label(nam[(uint)ocheck], yes[(uint)odeny]) ;
        switch(ret19)
        {
            case 'T': if (cdouble('T')) ret19 = 0; break;
            case 'F': ret19 = 0;                   break;
            case 'Z': odeny = ~odeny;            break;
        }
    }
    target = sn;
    tblink = fblink = 0 ;
    sn = getfvalue("   S.N.   ", (ulong)sn, 0, 0) ;
    if (sn == 0) sn = target;
    if (returncode){ 
        fblink = 1; phase = 20 ;
    }
    if(!returncode){ 
        ungetch(); fblink = 1; phase = 20 ; 
    }

    target = 0;
    fun21();
}

void fun20()                    // Waiting for confirmation before saving
{
    if (testbit(fblink)) puts("\n STORE");
    switch(getch())
    {
        case 'T':
        case 'F': fblink = 1 ;
            phase = 8 ;
            break ;
        case 'Z':
        case 'C':
            default : hometest() ; break ;
    }
}

void fun21()
{
    tblink = fblink = 0 ;
    if (onet){
        xnet = net;
        puts("\nNET-  ") ;                xbit(ffuse, 1) ;
        net = getvalue(net, 64, 5, 2) ; xbit(ffuse, 0) ;
        if (net == 0) net = xnet;
        if (returncode){
            fblink = 1 ;
            phase = 20 ;
        }    
        if(!returncode){
            ungetch()  ; 
            fblink = 1 ; 
            phase = 20 ;
        }
    }
    else{
        if (oprofi){
            profiadd = getfvalue("PR ADD", profiadd, 0,125) ;
            if (returncode){
                fblink = 1 ;
                phase = 20 ;
            }    
            if(!returncode){
                ungetch()  ; 
                fblink = 1 ; 
                phase = 20 ;
            }
        }
        else{
            fblink = 1 ;
            phase = 20 ;
        }
    }
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void  kdouble(uchar c)            // Double keystroke branching
{
    switch (c)
    {
        case 0: phase = 3;
            break;    // Z-Z: Identifier + options programming
        case 1: phase = 4;
            break;    // Z-T: Special parameters programming
        case 2: clrscr();backlight(0);/*(float)qtbl=(float)zqtbl*10;*/ // T-Z: Reboot through watchdog
            watchdog();tare=oldtare; while(1); break;
        case 3: phase = 5;
            break;    // T-T: Start calibration
        case 6: phase=3;
            break;
        default:;
        }
}

bit     cdouble(uchar c)
{
    timeout = hafsec ;
    while(timeout)
    {
        if ((kbhit())&&(getch()==c))
        {
            timeout = 0 ;
            return true ;
        }
        watchdog() ;
    }
    return false ;
}

void  hometest()              // Returning to phase 1, from any other phase
{
    phase = 1;
    pass = 0XC0;
    treqz = treqt = tblink = 0;
    fblink = 0;
    xbit(ftuse, 0) ;
    flag(fkilo, 1) ;
}

void  nextop()                // Displaying the next option
{
    phase = undisp(5);
    if (phase>=(sizeof(options)<<3)) phase = 0;
    if (options[phase>>3]&(1<<(phase&7))) printf("\4-%02u",(uint) phase+1);
    else printf("\4 %02u", (uint) phase+1);
}

void  shiftop()               // Shift the rightmost digit
{
    ascbuf[0] = unput(6) ;
    ascbuf[1] = '0' ;
    ascbuf[2] =  0  ;
    phase = atoi(ascbuf) ;
    if (phase > (sizeof(options)<<3)) phase = 1 ;
    if ((phase)&&(options[(phase-1)>>3]&(1<<((phase-1)&7))))
        printf("\4-%02u", (uint) phase);
    else printf("\4 %02u", (uint) phase);
}

void  nexthop()               // Displaying the next hidden option
{
    phase = undisp(5);
    if (phase>=(sizeof(hidop)<<3)) phase = 0;
    if (hidop[phase>>3]&(1<<(phase&7))) printf("\4-%02u",(uint) phase+1);
    else printf("\4 %02u", (uint) phase+1);
}

uchar i;
void  opsetch()
{
    if ((!opset)||(opset>51)) opset = 1 ;

    if (memcmp(options, opdef+(opset-1)*8, 8)){
        for (i = 0 ; i < 51 ; i++)
            if (!memcmp(options, opdef+(i)*8, 8))    
                opset = i+1;
    }

    puts("\nS.OP-  ") ;
    if (memcmp(options, opdef+(opset-1)*8, 8))
    {
        fblink = tblink = 0 ;
        while(!kbhit())
        {
            if (!tblink)
            {
                if (testbit(fblink)) puts("\5  ") ;
                else
                {
                    printf("\5%02d", (uint)opset) ;
                    fblink = 1 ;
                }
                tblink = hafsec ;
            }
            watchdog() ;
        }
    }
    else
    {
        printf("\5%02d", (uint)opset) ;
        while(!kbhit()) watchdog() ;
    }
}

uchar   eight()
{
    flag(fupl  , 1)  ;   // upper left corner
    flag(ftare , 1)  ;   // "TARE" indicator
    flag(fzero , 1)  ;   // "ZERO" indicator
    flag(flol  , 1)  ;   // lower left corner
    flag(flo   , 1)  ;   // "LO-" indicator
    flag(fbat  , 1)  ;   // "BAT" indicator
    flag(fmidl , 1)  ;   // middle left arrow
    flag(fmidr , 1)  ;   // middle right arrow
    flag(foz   , 1)  ;   // "-OZ" indicator
    flag(fpound, 1)  ;   // "LB" indicator
    flag(fupr  , 1)  ;   // upper right corner
    flag(fkilo , 1)  ;   // "KG" indicator
    flag(fpcs  , 1)  ;   // "PCS" indicator
    flag(flor  , 1)  ;   // lower right corner
    puts("\n888888") ;
    phase = getch()  ;
    clrscr() ;
    return(phase) ;
}
/*
            float   mload[] = { 0.1, 0.2, 0.3, 0.5 } ;
            */
double  floor(double) ;
ulong   lloor(ulong load)
{
    float   fload ;
    uint    t = 0 ;

    fload = load/fpow(10, decperiod) ;
    if (fload < 1.0)
    {
        while(t < 3)
        {
            if ((fload-0.05) < mload[t]) break  ; else t++ ;
        }
        fload = mload[t] ;
    }
    else
    {
        fload = floor(fload) ;
    }
    fload *= fpow(10, decperiod) ;
    return((ulong)fload) ;
}
void fun85()
{
    xdata uchar ret ,sw_yes_no,sw_c;
    code  uchar *nam[] = { " ZERO  "} ;
    code  uchar *yes[] = { "    NO", "  YES " } ;

    sw_yes_no=sw_c=0;
    while(!sw_c){
        ret = label(nam[0], yes[(uint)sw_yes_no]) ;
        switch(ret){
            case 'Z' : sw_yes_no^=1; break;
            case 'T' : sw_c=1 ;      break ;
            case 'F' : sw_c=1 ;      break ;
        }
    }
    if(sw_yes_no){
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
void fun86()
{
    xdata uchar ret ,sw_yes_no,sw_c;
    code  uchar *nam[] = { " TARE  "} ;
    code  uchar *yes[] = { "    NO", "  YES " } ;

    sw_yes_no=sw_c=0;
    while(!sw_c){
        ret = label(nam[0], yes[(uint)sw_yes_no]) ;
        switch(ret){
            case 'Z' : sw_yes_no^=1; break;
            case 'T' : sw_c=1 ;      break ;
            case 'F' : sw_c=1 ;      break ;
        }
    }
    if(sw_yes_no){
        if (transtare) transtare = 2 ;  // remote tare request
        treqt = 0;
        if (ocumt || !tare){
            if ((ofult)||(wdisp<=fiveperstop)){ // Restriction for tare
                tare = weight1 - zref;
                if (byte(tare,0)) tare = 0;
                if (tare<onediv)  tare = 0;
                if (osptar) xbit(xtare, on) ;  // relays start after tare
            }
        }else tare = 0;
    }
}

void funcon()
{
    xdata ulong tt;
    xdata float mwfactor;

    wprint=0;
    tblink=10;
    while (tblink) {puts("CON.GRO"); watchdog();}
        wprint  = getfvalue("CON.GRO", wprint, decim,0) ;
    if (returncode)
    {
        atod = atodin() ; weight(atod) ;
        zref = weight0 - (float)wprint/factor ;
        lastzref = zref ;
    }
    else
    {
        hometest();
        return;
    }
    tt  = getfvalue(" A-D0   ",(ulong)zref0>>8, 0,0) ;
    if (returncode)
    {
        zref0 = tt<<8;
        phase=8;
        fblink=1;
    }
    else hometest();
}

void fun88(void)
{
    sformat(icbuf, 0, decim); label("\  DOT ", icbuf);
    sprintf(icbuf, "%6.*f ",(uint)decim,(float) wstop/fpow(10, decim)); label("\ STOP  ", icbuf);
    sprintf(icbuf,"%6.*f ",(uint)decim,(float)round/fpow(10, decim)); label("\ ROUND ", icbuf);
    sprintf(icbuf, "\n%s", xbaudisp[com1&7]);
    phase = (com1>>4)&7; sprintf(icbuf+4, ".%s ", uformat[phase]);
    label("\n BAUD1", icbuf); ungetch(); puts(icbuf); timeout = 2;
    getch();
    sprintf(icbuf, "\n%s", xbaudisp[com2&7]);
    phase = (com1>>4)&7; sprintf(icbuf+4, ".%s ", uformat[phase]);
    label("\n BAUD2", icbuf); ungetch(); puts(icbuf); timeout = 2;
    getch();
    sprintf(icbuf, "%6.*f ",(uint)decim,(float) nforce/fpow(10, decim)); 
    label("\ FORCE ", icbuf);
    //sprintf(icbuf, "%6.1f ",(float) dforce); 
    sformat(icbuf, dforce, 1) ;
    label("\TFORCE ", icbuf);
    if (!odeny && !ocheck){
        sprintf(icbuf, "  %02d  ",(uint)setplim);
        label("\nS.P - M", icbuf);
    }
    if (onet){
        sprintf(icbuf, "  %02d  ",(uint)net);
        label("\n NET  ", icbuf);
    }
    if (oprofi){
        sprintf(icbuf, "  %02d  ",(uint)profiadd);
        label("\nPR ADD", icbuf);
    }
    if (bcd && ((!bcd420)&&(!bcdout))){
        switch(bcdisp){
            case 1 : puts("\nBCD.1-4") ; break ;
            case 2 : puts("\nBCD.2-5") ; break ;
            case 3 : puts("\nBCD.3-6") ; break ;
        }
        getch();
    }
}
