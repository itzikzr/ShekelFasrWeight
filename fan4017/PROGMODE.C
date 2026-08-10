#include "fanbase.h"

void    prog0(void);
void    prog1(void);
void    prog2(void);
void    prog3(void);
void    prog5(void);
void    prog6(void);
void    prog7(void);

void    OptionNumber();
void    PowerOff();
void    ForceZero();
void    TimeForceZero();
void    BackLight();
void    SetBaud();
void    drange();
void    ZeroLimit();
void    SerialNum();
void    bcd_show();
void    line_num();
void    tear_num();

void    ConGro();
void    Span();
void    ShowOffset();
void    ShowZref0();
void    ShowFactor();
void    bat_show();
void    profiaddres();
void    netaddres();

void progmode()
{
    dpcs = dkg = dlb = dbat = dzero = dtare = fsave = 0;
    if (comstat()) getcom();
    if (fun==NULL) fun = prog0;
    fun();
}

//********  IDENTEFIRE  *******************
void prog0()
{
    dml = dmr = 1;
    printf("\n=%5u", vernum);
    fun = prog1;
}


void prog1()
{    
    xdata uchar f=0;

    if (kbhit()) 
        switch (getch(0)){
            case 'T': fun = OptionNumber;
            case 'P': twait = 30;
                      while(twait){
                          if (kprint) f = 1;
                          else        twait = f = 0;
                          watchdog();  
                      }
                      if (f){fun=NULL; prog = 4;}
                      if (cdouble('P')) {fun=NULL; prog = 5;}
                      break;
            case 'Z': twait = 20;
                      while(twait){
                          if (kzero) f = 1;
                          else       twait = f = 0;
                          watchdog();  
                      }
                      if (f) fun = ConGro;
                      break;
        }
}

//********  DECIMAL POINT *****************
void prog2()
{
    xdata uchar k;

    dmr = dml = 1;
    k = disform & 0X7F;
    if (k>6) k = 0;
    else k = disform;
    hibyte(progvar) = k;
    brut = 0;
    display(brut, k);
    fun = prog3;
}

void prog3()
{
    xdata uchar k, k1, k2;
    xdata float f, k3;

    if (kzero){
        if (!timout){
            timout = hafsec;
            k = hibyte(progvar);
            if (!k      ) k  = 0X86;
            if (k & 0X80) k -= 0X81;
            else          k ^= 0X80;

            if (k){
                k = hibyte(progvar);
                if (!k) k  = 0X86;
                if (k & 0X80){k -= 0X81; k ^= 0X80;}
                else          k ^= 0X80;
            }

            hibyte(progvar) = k;
            brut = 0;
            display(brut, k);
//sprintf(ascbuf,"\n%d %d",(uint)disform,(uint)k); comstring(ascbuf); while(fsend);
        }
    }
    else timout = 0;
    if (kbhit()) k = getch(0);
    else         k = 0;
    switch (k) {
        case 'T': if (cdouble('T')){
                        k = hibyte(progvar);
                        if (k != disform) fprog = 1;
                        k1 = DecimalPointNum(disform);
                        k2 = DecimalPointNum(k);
                        if (k1 < k2){
                            k3 = (float)(k2 - k1);
                            f = fac2float(fac0) * fpow(10, k3);
                            fac0 = float2fac(f);
                        }
                        if (k1 > k2){
                            k3 = (float)(k1 - k2);
                            f = fac2float(fac0) / fpow(10, k3);
                            fac0 = float2fac(f);
                        }
                        disform = k;    
                        if (fprog) gosave();
                        else       reboot();
                  }    
                  else{ 
                        k = hibyte(progvar);
                        k1 = DecimalPointNum(disform);
                        k2 = DecimalPointNum(k);
                        if (k != disform) fprog = 1;
                        if (k1 < k2){
                            k3 = (float)(k2 - k1);
                            f = fac2float(fac0) * fpow(10, k3);
                            fac0 = float2fac(f);
                        }
                        if (k1 > k2){
                            k3 = (float)(k1 - k2);
                            f = fac2float(fac0) / fpow(10, k3);
                            fac0 = float2fac(f);
                        }
                        disform = k;
                        fun = prog5;
                  }
        case 'Z': break;
        case 'P': reboot(); break;
        default:;
    }
}

//********  OPTION SET  *******************
bit     CheckOpOn(uchar);
bit     CheckOpOn2(uchar);
void    CheckSetNumber();
void    SetNumber();
bit     fop;

void OptionNumber()
{
    xdata uchar i=0, j=0, ret, n;
    xdata ulong l;

    CheckSetNumber();
    SetNumber();
    if (twait){
        if ((ulong)option  != (ulong)novop ) fprog = 1; 
        if ((ulong)option2 != (ulong)novop2) fprog = 1;
        novop  = option;
        novop2 = option2;
        if (fprog) gosave();
        else       reboot();
        return;
    }  
    if (CheckOpOn(i)) j = 1;
    else              j = 0;
    fop = 1;

    printf("\nOP.%02u-%1u",(uint)i+1,(uint)j);
    do{
        if (kbhit()){
            ret = getch(0);
            switch(ret){
                case 'Z': if(cdouble('Z'))
                          {
                              j = j ^ 1;
                              l = 1;                              
                              if (i > 31){l <<= i-32; option2 = option2 ^ l;}
                              else       {l <<= i   ; option  = option  ^ l;}
                              printf("\nOP.%02u-%1u",(uint)i+1,(uint)j);
                          }
                          else{
                              n = timeout = 0;
                              do{
                                  watchdog();
                                  if (!timeout){
                                      timeout = 3;
                                      if (i >= (opnum-1)) i = 0; else i++;
                                      if (i < 32){
                                          if (CheckOpOn(i)) j = 1;
                                          else              j = 0;
                                      }else{
                                          if (CheckOpOn2(i-32)) j = 1;
                                          else                  j = 0;
                                      }  
                                      printf("\nOP.%02u-%1u",(uint)i+1,(uint)j);
                                  }
                              }
                              while(kzero);
                          }
                          break ;
                case 'T': if (cdouble('T')){
                              if ((ulong)option  != (ulong)novop ) fprog = 1; 
                              if ((ulong)option2 != (ulong)novop2) fprog = 1;
                              novop  = option;
                              novop2 = option2;
                              if (fprog) gosave();
                              else       reboot();
                          }
                          else fun = PowerOff;
                          if ((ulong)option  != (ulong)novop ) fprog = 1; 
                          if ((ulong)option2 != (ulong)novop2) fprog = 1; 
                          novop  = option;
                          novop2 = option2;
                          return;
                }
        }
        watchdog();
    }
    while(1);
}

void SetNumber()
{
    xdata uchar p[4];
    xdata ulong l,j,n,l1,j1;

    if ((!opset)||(opset>51)) opset = 1 ;

    j = (ulong)novop;
    memcpy(p,opdef+(opset-1)*8,4);
    l = ConvertLongByte(p);

    j1 = (ulong)novop2;
    memcpy(p,opdef+((opset-1)*8)+4,4);
    l1 = ConvertLongByte(p);

    if ((j != l) || (j1 != l1)){
        tblink = 0;
        twait = 8;
        do{
            printf("\nS.OP-%02d",(uint)opset);
            while(twait){
                if (kbhit()) break;
                watchdog();
            }
            tblink = 5;
            printf("\nS.OP-  ",(uint)opset);
            while(tblink)
            {
                if (kbhit()) break;
                watchdog();
            }
            twait = 8;
            watchdog() ;
        }
        while(!kbhit());
    }
    n = (ulong)opset;
    n = (ulong)getvalue("S.OP-",n ,0 ,51);
    if (!n){
        option  = (ulong)novop;
        option2 = (ulong)novop2;
        if(ktare) twait = 10;
        else      twait = 0;    
    }
    else{
        if ((ulong)opset != n) fprog = 1;
        memcpy(p,opdef+(n-1)*8,4);
        option = ConvertLongByte(p);
        memcpy(p,opdef+((n-1)*8)+4,4);
        option2 = ConvertLongByte(p);
        if (!option && !option2) SetNumber();
        else opset = (uchar)n;
    }
}

bit CheckOpOn(uchar i)
{
    xdata ulong x, y=1;

    x = option;
    y <<= i;
    if (x&y) return 1;
    else     return 0;
}

bit CheckOpOn2(uchar i)
{
    xdata ulong x, y=1;

    x = option2;
    y <<= i;
    if (x&y) return 1;
    else     return 0;
}

void CheckSetNumber()
{
    xdata uchar p[4], i;
    xdata ulong l,j,l1,j1;

    j = (ulong)novop;
    memcpy(p,opdef+(opset-1)*8,4);
    l = ConvertLongByte(p);

    j1 = (ulong)novop2;
    memcpy(p,opdef+((opset-1)*8)+4,4);
    l1 = ConvertLongByte(p);

    if ((j != l) || (j1 != l1)){
        for (i=1 ; i<=51 ; i++){
            memcpy(p,opdef+(i-1)*8,4);
            l = ConvertLongByte(p);
            memcpy(p,opdef+((i-1)*8)+4,4);
            l1 = ConvertLongByte(p);
            if ((j == l) && (j1 == l1)){
                opset = i;
                return;
            }
        }
    }
}

//***********  POWER OFF ****************

void PowerOff()
{
    xdata uchar k;
//sprintf(ascbuf,"\n%ld",option); comstring(ascbuf); while(fsend);
    k = poff;
    if (k > 99) k = 0;
    k = getdata("\nP-OFF ",(ulong)k, 0, 99);
    if (k != poff) fprog = 1;
    poff = k;
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else{ 
            sprintf(ascbuf, "\n%7.*f",(uint)DecimalPointNum(disform), 0);
            blink(" DOT  ", ascbuf, 10) ;
            fun = prog2;
        }
    }
    if (kprint) reboot();
}


//********  FULL SCALE  *******************
void prog5()
{
    progvar = full0;
    hibyte(progvar) = 0;
    progvar = getdata("\n STOP ", progvar, disform, 0);
    hibyte(progvar) = round0;
    if (progvar != full0) fprog = 1;
    full0 = progvar;

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = prog6;
    }
    if (kprint) reboot();
}

//***********  FORCE ZERO & TFORCE ZERO **********

void ForceZero()
{
    xdata ulong x;

    x = (ulong)mforce;
    x = getdata("\nFORCE ",x, disform, 65000);
    if (mforce != (uint)x) fprog = 1;
    mforce = (uint)x;
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
            return;
        }else{
            if (mforce) fun = TimeForceZero;
            else        fun = BackLight;
        }
    }
    if (kprint) reboot();
}

void TimeForceZero()
{
    xdata ulong x;

    if (mforce)
    {
        x = (ulong)tforce0;
        x = getdata("\nTFORCE",x, 133, 240);
        if (tforce0 != (uchar)x) fprog = 1;
        tforce0 = (uchar)x;
        if (ktare){
            if (cdouble('T')){
                if (fprog) gosave();
                else       reboot();
            }
            else fun = BackLight;
        }
        if (kprint) reboot();
    }else fun = BackLight;
}

//***********  ROUND  *****************
void prog6()
{
    xdata ulong x=0,l=0,f=1;

    do{
        progvar = 0;
        inbyte(progvar, 3) = round0;
        //if (disform == 133) progvar = getdata("\n ROUND", progvar, disform, 5 );
        //else                
        progvar = getdata("\n ROUND", progvar, disform, 500);
        watchdog();
    }while(!progvar);
    
    if (round0!=inbyte(progvar, 3)) fprog = 1;
    round0 = inbyte(progvar, 3);

    if (ktare){
        if (cdouble('T')){
            if (fprog) {
                inbyte(x, 3) = round0 ;            
                inbyte(l, 3) = drange0;
                gosave();
            }else reboot();
        }
        else fun = drange;
    }
    if (kprint) reboot();
}
/*
void prog6()
{
    xdata ulong x=0,l=0;

    progvar = 0;
    inbyte(progvar, 3) = round0;
    progvar = getdata("\n ROUND", progvar, disform, 250);
    if (round0!=inbyte(progvar, 3)) fprog = 1;
    round0 = inbyte(progvar, 3);

    if (ktare){
        if (cdouble('T')){
            if (fprog) {
                inbyte(x, 3) = round0 ;            
                inbyte(l, 3) = drange0;
                gosave();
            }else reboot();
        }
        else fun = drange;
    }
    if (kprint) reboot();
}
*/
//***********  double range limit **********
void drange()
{
    xdata ulong x=0,l=0;

    progvar = 0;
    inbyte(progvar, 3) = drange0;
    progvar = getdata("\n  RAN ", progvar, 0, 99);
    if (drange0 != inbyte(progvar, 3)) fprog = 1;
    drange0 = inbyte(progvar, 3);

    if (ktare){
        if (cdouble('T')){
            if (fprog) {
                inbyte(x, 3) = round0 ;            
                inbyte(l, 3) = drange0;  
                gosave();
            }else       reboot();
        }
        else fun = ForceZero;
    }
    if (kprint) reboot();

}

//***********  BACK LIGHT **********
void BackLight()
{
    xdata ulong x;

    x = (ulong)backl0;
    x = getdata("\nB.LIGHT",x, 133, 999);
    if (backl0 != (uint)x) fprog = 1;
    backl0 = (uint)x;

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = SetBaud;
    }
    if (kprint) reboot();
}

//***********  BAUD RATE **********
void SetBaud()
{
    xdata uchar ret,s[10];
    xdata uchar i, j, y ,x,wbaud1,f=1;

    i = rsform & 0x0f;   // baud
    j = rsform & 0x70;   // j=0 N8, j=1 N7, j=2 E8, j=4 E7, j=6 O8, j=7 O7
    j = j >> 4;

    wbaud1 = rsform;
    sprintf(s, "\n %s.%s",bformat[i],uformat[j]);
    blink("BAUD1 ", s, 10) ;
    printf("\n %s.%s",bformat[i],uformat[j]);
    do{
        if (kbhit()){
            ret = getch(0);
            switch(ret){
                case 'Z': timeout = 0;
                          do{
                              watchdog();
                              if (!timeout){
                                  timeout = 4;
                                  if (j >= 7){
                                      j = 0;
                                      if (i >= 9) i = 0; else i++;
                                  }
                                  else j++;
                                  if (j == 2) j++;
                                  if (j == 6) j++;
                                  printf("\n %s.%s",bformat[i],uformat[j]);
                                  y = x = 0;
                                  x = i;    y = j;
                                  y <<= 4;  y |= x;
//sprintf(ascbuf,"\n%d %d %d %s.%s",(uint)i, (uint)j,(uint)y,bformat[i],uformat[j]); comstring(ascbuf); while(fsend);
                                  wbaud1 = y;
                              }
                          }
                          while(kzero);
                          break ;
                case 'T': if (ktare){
                              if (cdouble('T')){
                                  if (rsform != wbaud1) fprog = 1;
                                  rsform = wbaud1;
                                  if (fprog) gosave();
                                  else       reboot();
                              }
                              else fun = ZeroLimit;
                          }
                          f = 0;
            }
        }
        watchdog();
    }
    while(f);
    if (rsform != wbaud1) fprog = 1;
    rsform = wbaud1;
}

//***********  double range limit (%) AND SAVE **********
void prog7()
{
    // last programming phase

    if (fnov){
        if (!(novop2 & 0x00004)) auto0 = 0; //op 35
        else                     auto0 = 0X20;
        novsave();
        puts("\nS DONE");
        pause(10);

        home();
        prog = 3;
    }else{
        if (fprog) gosave();        
        else       reboot();
    }
}

void ZeroLimit()
{
    xdata ulong x;

    progvar = full0;
    hibyte(progvar) = 0;

    x = (ulong)stop0;
    if (x > progvar) x = progvar;
    x = getdata("\nZERO R",x, disform, progvar);
    if (stop0 != (uint)x) fprog = 1;
    stop0 = x;

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }else{
//            temp = (ulong)bcdisp;
            fun = SerialNum;//bcd_show;
        }
    }
    if (kprint) reboot();
}

void SerialNum()
{
    progvar = (ulong)sn;
    progvar = getdata("\n  S.N  ",progvar, 0, 0);
    if (sn != progvar) fprog = 1;
    sn = progvar;

    temp = (ulong)bcdisp;
    if (ktare ) fun = bcd_show;
    if (kprint) reboot();
}

void profiaddres()
{
    xdata ulong x;

//    if (!(option2 & 0x02000) || (option2 & 0x10000)){ // op 46 profi bus or net
    if (!(option2 & 0x02000)){ // op 46 profi bus
        fun = netaddres;
        return;
    }

    progvar = (ulong)profiadd;
    x = getdata("\nPR ADD",progvar, 0, 125);
    if ((ulong)profiadd != x) fprog = 1;
    profiadd = (uchar)x;

    if (ktare ) fun = netaddres;
    if (kprint) reboot();
}

void netaddres()
{
    xdata uint x;

    if (!(option2 & 0x10000)){  // op 48 net
        fun = prog7;
        return;
    }

    progvar = net;
    x = getvalue("NET-",progvar ,0 ,99);
    if (net != x && x) fprog = 1;
    if (x) net = x;

    if (ktare  || x || fprog) fun = prog7;
    if (kprint || (!x && !fprog)) reboot();
}

//***********  BCD **********
void bcd_show()
{
    if (!(option2 & 0x00008)){ // op 36 bcd on
        temp = (ulong)batisp;
        fun = bat_show;
        return;
    }

    if ((!bcdisp)||(bcdisp>3)) bcdisp = 1;
    switch(bcdisp){
        case 1 : puts("\nBCD.1-4") ; break ;
        case 2 : puts("\nBCD.2-5") ; break ;
        case 3 : puts("\nBCD.3-6") ; break ;
    }   
    if (kbhit()){
        switch (getch(0)){
            case 'Z': bcdisp++;    break;
            case 'T': if (temp != (ulong)bcdisp) fprog = 1;
                      if (cdouble('T')){
                          if (fprog) gosave();
                          else       reboot();
                      }else{
                           temp = (ulong)batisp;
                           fun = bat_show;
                      }
                      break;
            case 'P': reboot();
        }
    }
}

//***********  BAT **********
void bat_show()
{
    if (!(option & 0x400000)){ // op 23 bat on
        fun = line_num;
        return;
    }

    if ((!batisp)||(batisp>3)) batisp = 1;
    switch(batisp){
        case 1 : puts("\nBAT 12") ; break ;
        case 2 : puts("\nBAT  9") ; break ;
        case 3 : puts("\nBAT  6") ; break ;
    }   
    if (kbhit()){
        switch (getch(0)){
            case 'Z': batisp++;    break;
            case 'T': if (temp != (ulong)batisp) fprog = 1;
                      if (cdouble('T')){
                          if (fprog) gosave();
                          else       reboot();
                      }
                      fun = line_num; // op 13
                      break;
            case 'P': reboot();
        }
    }
}

void line_num()
{
    xdata ulong x;

    if (option & 0x1000){ // op 13
        fun = tear_num;
        return;
    }

    progvar = (ulong)line0;
    x = getdata("\n LINE ",progvar, 0, 250);
    if (line0 != (uint)x) fprog = 1;
    line0 = x;

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else{ 
            fun = tear_num;
        }
    }
    if (kprint) reboot();
}

void tear_num()
{
    xdata ulong x;

    if (!(option2 & 0x00200)){  // op 42 
        fun = profiaddres;
        return;
    }

    progvar = (ulong)tear;
    x = getdata("\n TEAR ",progvar, 0, 250);
    if (tear != (uint)x) fprog = 1;
    tear = x;

    if (ktare) fun = profiaddres;
    if (kprint) reboot();
}

/***************************************************************************************/

void ConGro()
{
    temp = fprog = 0;
    temp = getdata("\nCON.GRO",temp, disform, 0);
    if (temp > 0){
        if (zref1 >= temp){
//sprintf(ascbuf,"\n2  %5ld %5ld %5ld ",brut, temp,  progvar); comstring(ascbuf); while(fsend);    
            zref1 = brut;
            zref1 -= temp;
            fprog = 1;
        }
        else{
            puts("\nERROR ");
            pause(20);
            reboot();
        }
    }
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else{
            if (!fprog) fun = Span;
            else        fun = ShowOffset;
        }
    }
    if (kprint) reboot();
}

void Span()
{
    temp = fprog = progvar = 0;
    temp = getdata("\n SPAN ",temp, disform, 0);
    if (temp){
        if (temp > zref1){
            puts ("\nCAL SP");
            timeout = 2;
            while(timeout){
                if (atod(0)) atod2weight(brut, 1);
                watchdog();
            }
            progvar = brut;
//sprintf(ascbuf,"\n1  %5ld %5ld %5ld ",brut, temp,  progvar); comstring(ascbuf); while(fsend);    
    
            hibyte(zref0) = offset0 + 1;
            novsave();
        
            adcom(0, adset);
            if (!adini());
            timeout = 2;
            while(timeout){
                if (atod(0)) atod2weight(brut, 1);
                watchdog();
            }
            progvar = brut - progvar;
//sprintf(ascbuf,"\n2  %5ld %5ld %5ld ",brut, temp,  progvar); comstring(ascbuf); while(fsend);    

            offset0 += (uchar)((float)(temp) / (float)progvar);
            offset0 -= (uchar)((float)(brut) / (float)progvar);
            hibyte(zref0) = offset0 + 1 ;
            novsave();
    
            adcom(0, adset);
            if (!adini());
            timeout = 2;
            while(timeout){
                if (atod(0)) atod2weight(brut, 1);
                watchdog();
            }
            zref1 = brut - temp;
//sprintf(ascbuf,"\n4  %5ld %5ld %5ld \n\n",brut, temp, zref1); comstring(ascbuf); while(fsend);
            novsave();
        }else{
            puts ("\nCAL CO");
            fprog = 1;
            zref1 -= temp;
            pause(10);
        }
    }
    fun = ShowOffset;
}

void ShowOffset()
{
    xdata uchar x;

    x = offset0;
    x = (uchar)getdata("\nOFFSET", (ulong)x, 0, 255);

    if (x != offset0){
        hibyte(zref0) = offset0 = x;
        fprog   = 1;
    }
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = ShowZref0;
    }
    if (kprint) reboot();
}

void ShowZref0()
{
    xdata ulong x;
    xdata uchar c;

    temp = zref0;
    c = hibyte(zref0);
    temp >>= 8;
    hiword(temp) = 0;
    x = temp - 32767; 
    temp = getdata("\nZREF 0", temp-32767, 0, 65536);
    
    if (temp != x){
        temp += 32767;
        zref0 = temp << 8;
        hibyte(zref0) = c;
        fprog = 1;
    }

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = ShowFactor;
    }
    if (kprint) reboot();
}

void ShowFactor()
{
    xdata float f, n, l;
    xdata uchar c;

    c = DecimalPointNum(disform);
    switch(c){
        case 0: l = fac2float(fac0-0X3000000) * 1000; break;
        case 1: l = fac2float(fac0-0X3000000) * 100 ; break;
        case 2: l = fac2float(fac0-0X3000000) * 10  ; break;
        case 3: l = fac2float(fac0-0X3000000)       ; break;
        case 4: l = fac2float(fac0-0X3000000) / 10  ; break;
        case 5: l = fac2float(fac0-0X3000000) / 100 ; break;
    }
    if (full1) n = (float)full1 / (float)(fpow(10, c) * 10);
    else       n = 1;
    f = l / n;

    sprintf(ascbuf,"%f",f);
    blink("\nFACTOR", ascbuf, 10);
    temp = 0;    
    if (kzero) temp = getdata("\nFACTOR", temp, 129, 999999);
    if (temp){
        fprog = 1;
        f = ((float)temp / fpow(10, 5)) * n;
        switch(c){
            case 0: fac0 = float2fac(f / 1000) ; break;
            case 1: fac0 = float2fac(f / 100 ) ; break;
            case 2: fac0 = float2fac(f / 10  ) ; break;
            case 3: fac0 = float2fac(f * 1   ) ; break;
            case 4: fac0 = float2fac(f * 10  ) ; break;
            case 5: fac0 = float2fac(f * 100 ) ; break;
        }
        fac0 += 0X3000000;
    }
    if (ktare){
        if (fprog) gosave();
        else       reboot();
    }
    if (kprint) reboot();
}


