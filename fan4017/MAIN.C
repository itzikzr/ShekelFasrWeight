#include "fanbase.h"

void    force_zero();
void    zero_tare_print_press();
void    All_Fanctiom();
void    blight();
void    weight_display();
void    show_atod();
void    show_factor();
void    show_bcd();
void    check_option();
void    show_weight();
void    PowerOffm();
void    ZeroPrint_function(void);

void    battery(void)
{
    uchar f ;

    fbat = 0;
    if (is_battery != 1955){
        is_battery  = 1955;               // battery's signature
        twait = 30;
        tblink = f = 0;
        if (opbatdisp){
            while(twait){
                watchdog();
                if (tblink == 0) {
                    tblink = hafsec ;
                    f ^= 1;
                    if (!f) puts  ("\n      ");
                    else    printf("\n BAT  ");
                }
            }
        }       
        gtotal = ptotal = netto = 0;
        order = poffcounter = tall = num = 0;
        tare = 0;
    }else{
        if (ocount){
            progvar = *(ulong*)(kunit);
            if (countfac(0, progvar)) dpcs = 1;
        }
    
        tare = getare(0, tare);
    }
    
    if (fmerror) reboot();
}
double fabs (double) ;
void    main()
{
    uchar k;

    if (fbat) battery() ;						  
    if (kbhit()){
        keyboard(getch(0));
        tforce = tforce0;
    }
    
    if (atod(0)){
        if (testbit(ferror)) errors(errnum);
        else{
            if (fpatod){
                display(fan2gul(brut), 0);
                return;
            }

            if (adtype[0] == 1) k = disform;
            if (adtype[0] == 2){
                brut += 0X80;
                brut >>= 8;
                if (inbyte(brut, 1)) hibyte(brut)--;
                k = 0;
            }
            if (fmerror) reboot();
            All_Fanctiom();
            
            if (tsend){
                puts("\n SEND ");
            }else{
                if (!fhold) weight_display();
            }
            
            temp  = labs(brut);
            if (funder0 && !weight0 && szero) temp = szero;
            if (((round0*5) <= temp) && (fngz || funder || (weight0 < szero) || hibyte(brut))){
                bcdbuf = bcd(0, 2);
            }else{
                if ((!bcdsel||funder0)&&((funder||fngz)||(weight0<szero))) bcdbuf = bcd(0   , 1); 
                else                                                       bcdbuf = bcd(brut, 1); 
            }
	
            if (!fstab) tprint = 10;
            if (!funder && !tprint && oautom && fnew && brut && brut >= zerop) {
                if (!otrans) Print(2);    // Automatic printing on stabel
                else{
                    if (onet) wttrans();
                    else      SendWeightToCom(brut);
                }
            } 
            if (freqw){
                if (onostab)    SendWeightToCom(brut);
                //else if (fstab) SendWeightToCom(brut);
            }
            if (opbat && !tbat){
                tbat = tensec;
                check_bat(getbat(0));
            }
        }
        tref = 3;
        neww = brut;
    }else refresh(0);

    if (!osetpoint) {
        if (oconti) SendWeightToCom(brut);
        else{
			do{
				if (comstat()) serial(); 
				watchdog();
			}while(comstat());		
			//if (comstat()) serial(); 
		}
    }
}

void All_Fanctiom()
{
    if (adstat[0] & 0x01) fstab = 0;
    else                  fstab = 1;
    force_zero();
    zero_tare_print_press();
    blight();
    if (poff){
        power_off();
        if (!fstab) power_off_reset();
    }
		
		if (opPrint) fnew = 1;
		else{
			if (ohalf){ 
					if ((netto/2) >= weight) fnew = 1;
			}else{
					if (weight != netto) fnew = 1;
			}
			if (fnew && ftkeyp) ftkeyp = fnew = 0;		
		}
}

void weight_display()
{    
    if (fone) return;
    if (!odisp){
//        if (!tdisplay)
        {
            if (dpcs){
                if (!status(0)){
                    if (fstab){
                        display(brut, 0);
                        olddisp = brut;
                    }
                }
            }else{
                if (!status(0)){
                    if (!funder || !oneg) {
                        if (fstab){
                            display(brut, disform);
                            olddisp = brut;
                        }
                    }
                    fngz = 0;
                }else{
                    if (!oneg){
                        if (szero > weight0){ 
                            brut = (szero - weight0);
                            fngz = 1;
                        }
                        if (!fover) {
                            if (fstab){
                                display(brut, disform);
                                olddisp = brut;
                            }
                        }
                    }
                }
            }
            tdisplay = 1;
        }
    }else{
//        if (!tdisplay)
        {
            if (dpcs){
                if (!status(0)) display(brut, 0);
            }else{
                if (!status(0)){
                    if (!funder || !oneg) display(brut, disform);
                    fngz = 0;
                }else{
                    if (!oneg){
                        if (szero > weight0){ 
                            brut = (szero - weight0);
                            fngz = 1;
                        }
                        if (!fover) display(brut, disform);
                    }
                }
            }
        }
    } 
//    tdisplay = 1;
    weight = brut;
    if (osetpoint) setpoint();
}

void keyboard(uchar k)
{
    xdata uchar ret, f=0;

    switch (k){
        case 'Z': 
/*            
        dpcs = countmode(0, dpcs ^ 1);
        for (f=0;f<3;f++){
            if (atod(0)){
                if (testbit(ferror)) errors(errnum);
                else{
                    if (adtype[0] == 1) k = disform;
                    if (adtype[0] == 2){
                        brut += 0X80;
                        brut >>= 8;
                        if (inbyte(brut, 1)) hibyte(brut)--;
                        k = 0;
                    }
                }
            }
//sprintf(ascbuf,"\n%d %d %ld",(uint)f,(uint)dpcs, brut); comstring(ascbuf); while(fsend);
        }
//sprintf(ascbuf,"\n %d %ld",(uint)dpcs, brut); comstring(ascbuf); while(fsend);
        break;
*/        
        tall = blon = 0;
                  if (fhold){
                      fhold = dmr = dml = dur = dul = dlr = dll = 0;
                      break;
                  }
                  if (/*funder || */fover) break;
                  twait = 30;
                  while(twait){
                     watchdog();
                     if (kzero) f = 1;
                     else       f = twait = 0;
                  }
                  if (f){
                      f = 0; puts("\n ZERO ");
                      twait = 30;
                      while(twait){
                          watchdog();
                          if (cdouble('Z')) 
                             if (cdouble('Z')) {f = 1; twait = 0;}
                          nop();
                      }
                      if (f) treqz = twosec;
                      break;
                  }
//sprintf(ascbuf,"\n1 %ld %ld %ld %ld",brut,wtare(tare),stop0,(brut+wtare(tare))); comstring(ascbuf); while(fsend);
                  if (((brut+wtare(tare)) <= stop0) || hibyte(brut)/*fngz*/) treqz = trisec;
                  break;

        case 'T': tbl = onesec; 
                  tall = blon = 0;
                  if (fhold){
                      fhold = dmr = dml = dur = dul = dlr = dll = 0;
                      break;
                  }
                  if (!cdouble('T')) {
                      if (funder || fover || fngz) break;
                      if (!oetare){
                          if (omant && fzero && !dtare){
                               dml = dmr = 1;
                               progvar = getdata("\n TARE ", 0, disform, 0);
                               tare = tareweight(0, (float)progvar);
//sprintf(ascbuf,"\n1 %ld   %ld",tare, progvar); comstring(ascbuf); while(fsend);
                               dml = dmr = 0;
                          }else treqt = twosec;
                      }else{
                          twait = 30;
                          while(twait){
                              watchdog();
                              if (ktare) f = 1;
                              else       f = twait = 0;
                          }
                          if (f){
                              f = 0; 
                              puts("\n TARE ");
                              twait = 30;
                              while(twait){
                                  watchdog();
                                  if (cdouble('T')) 
                                      if (cdouble('T')) {
                                          f = 1; 
                                          twait = 0;
                                      }
                                  nop();
                              }
                              if (f) treqt = twosec;
                          }
                      }
                      break;
                  } 

        case 'F': clrscr(); dml = dmr = 1;
                  switch(getvalue("FUN-",0,0,16)){
                      case 0 : if (kprint) subdisp();  break;
                      case 1 : set_time_end_date();    break;
                      case 2 : subdisp();              break;
                      case 3 : clear_all();            break;
                      case 4 : show_factor();          break;
                      case 5 : show_bcd();             break;
                      case 6 : check_option();         break;
                      case 7 : show_weight();          break;
                      case 8 : show_atod();            break;
                      case 9 : reboot();               break;
                      case 10: UnitWeight();           break;
                      case 11: if (!opvol) calib420(); break;
                      case 12: if (opvol)  calib05() ; break;
                      case 13: rel_function();         break;
                      case 14: manual_sp();            break;
                      case 15: PowerOffm();            break;
                      case 16: ZeroPrint_function();   break;
                  }   dml = dmr = 0;                   break;

        case 'P': if (onet){
                      wttrans();
                      break;
                  }
                  if (osetpoint){
                      dmr = dml = 1;
                      define_sp();
                      dmr = dml = 0;
                      break;
                  }else if (opbigdsp && !ocount) break;
                  tbl = onesec;
                  tall = blon = 0;
                  if (ophold && !ocount){
                      fhold ^= 1;
                      if (fhold) dmr = dml = dur = dul = dlr = dll = 1;
                      else       dmr = dml = dur = dul = dlr = dll = 0;
                      break;
                  }
                  if (ocount){
                      twait = 15;
                      ret   = 0 ;
                      while(kprint){
                          if (!twait){
                              Count(); 
                              ret = 1; 
                              break;
                          }
                          watchdog();
                      }
                      dpcs = countmode(0, dpcs ^ 1);
                      if (opound) dlb = dpcs ^ 1;
                      if (okilo ) dkg = dpcs ^ 1;
                  }else {
                      if (cdouble('P') && !otrans){
                            if (olabel) subwgt();  // label total print
                            else        Print(1);  // total print
                      }
                      else{
                        tsend = 5; 
                        treqp = twosec;
                     }
                  }
                  break;
    }
    dtare = tare;
}

void zero_tare_print_press()
{
    long t;
    uchar f=0;

    weight0 = brut;
    fone = 0;
    if (treqp && (onostab || fstab)){
        if (otrans) freqw = 1;
        else if (!oconti) Print(2);  // row print
        treqp = 0;
    }

if (treqz)
{
//sprintf(ascbuf,"\n1111 %d %d %d %d",(uint)treqz,(uint)fstab,(uint)tstab,(uint)onostabTZ); comstring(ascbuf); while(fsend);
}
    if (treqz && ((fstab && !tstab) || onostabTZ)){
        if (fover){
            if (ftzero && opwiz) SendZeroReq(0);
            if (ftranzero) answer(nak1);
        }else{
            if (!opgetze || ocount || drange0){
//sprintf(ascbuf,"\n22 %d %d %d %d",(uint)treqz,(uint)fstab,(uint)tstab,(uint)onostabTZ); comstring(ascbuf); while(fsend);
                 progvar = getzer(0 ,1);
            }else{
                if (!hibyte(weight0)){
                    zref1 = szero = labs(weight0+wtare(tare));
                    novsave();
                }else{
                    if (opgetze){
                        progvar = getzer(0 ,1);
                        szero = 0; 
                        fmakez = 1;
                    }
                }
            }
            if (ftzero && opwiz) SendZeroReq(1);
            if (ftranzero) answer(ack1) ;
        }
        brut = treqz = 0;
        tprint = 10;
        fone = 1;
    }
//sprintf(ascbuf,"\n1 %d %d %d %d",(uint)treqz,(uint)fstab,(uint)tstab,(uint)onostabTZ); comstring(ascbuf); while(fsend);

    if (!treqz && ftzero && opwiz) SendZeroReq(0);
    if (!treqz && ftranzero) answer(nak1);
	if (!treqt && ftranstare) answer(nak1);
    if (treqt && (fstab || onostabTZ)){
//sprintf(ascbuf,"\n\n-------\n\n"); comstring(ascbuf); while(fsend);
        if (fngz || funder) t = brut * (-1);
        else                t = brut;
        if (fzero || ((wstop * 0.05) >= (t + wtare(tare)))) f = 1;  //  5% of 'wstop' tare
        if (ofult || f){
            if (ocumt){
//sprintf(ascbuf,"\n\n1 %ld  %f\n\n",tare, (float)(weight + wtare(tare))); comstring(ascbuf); while(fsend);
                if (!ocount && opgetze) tare = tareweight(0, (float)(weight + wtare(tare)));
                else                    tare = getare(0, (float)0X80000000);
            }else{
                if (!ocount && opgetze && !tare) tare = tareweight(0, (float)weight);
                else                             tare = getare(0, (float)0X81000000);
//sprintf(ascbuf,"\n2 %ld  %ld",tare, weight); comstring(ascbuf); while(fsend);
            }
            if (ftranstare){
                //if (tare) 
				answer(ack1) ;
                //else      answer(nak1);
            }
        }else if (ftranstare) answer(nak1);
        treqt = brut = 0;
        fone = 1;
    }
    if (szero && !fone) brut -= szero;
    if (!fstab) tstab = 5;
}

void blight()
{
    if (backl0 == 0 || backl0 == 999) return;
    if (!fstab){
        tbl = onesec;
        tall = blon = 0;
        return;
    }
    if (!tbl){
        tbl = onesec;
        tall++;
        if (tall == backl) blon = tall = 1;
    }
}

uint bcd(ulong w, uchar t)
{
    xdata float x;
    xdata uint  y;
    xdata uchar s[10] ;

    if (!opbcd){
        x = (float)w / (float)wstop;
        y = x * 0x0fff;
        if (fzero ) y =  0;
        if (bcdpol) y =~ y;
        if (t && ((t==2) || fover)){
            if (bcdini) y = 0;
            else        y = 0x0fff;
        }
//sprintf(ascbuf,"\n%06ld %06ld %x", brut,w,y); comstring(ascbuf); while(fsend);
    }else{
        sprintf(ascbuf, "%06ld", labs(w)) ;
        strncpy(s, ascbuf+bcdisp-1, 4) ; s[4] = 0 ;
        y  = ((uint)(s[0]-'0')) << 12 ;       // #2
        y += ((uint)(s[1]-'0')) <<  8 ;       // #3
        y += ((uint)(s[2]-'0')) <<  4 ;       // #4
        y += ((uint)(s[3]-'0')) ;             // #5
        if (bcdsel){
            if (y > 0x7999) y  = 0x7999 ;
            if (fngz || funder)   y |= 0x8000 ;
            if (t && ((t==2) || fover))  y = bcdini ? 0 :-1 ;
            if (bcdpol)                  y = ~y ;
        }else{
            if (t && ((t==2) || fover)) y = bcdini ? 0 :-1 ;
        }
        if (bcdpol && (bcdsel || !fover)) y = ~y;
    }
    return (y) ;
}

void show_atod()
{
    adcom(0, adraw);
    while (!fkey){
        watchdog();
        if (atod(0)){
            switch (errnum)
            {
                case 2:  puts("\nERR  2"); break ;
                case 3:  puts("\nERR  3"); break ;
                default: display(fan2gul(brut), 0);
            }
            errnum = 0;
        }
    }
    getch(0);
    adcom(0, adwey);
}

void show_factor()
{
    xdata float f, l;
    xdata uchar n;

    sprintf(ascbuf,"%6d",(uint)offset0);
    blink("OFFSET", ascbuf, 10);
    getch(0);

    temp = zref2;
    temp >>= 8;
    hiword(temp) = 0;
    temp -= 32767; 
    sprintf(ascbuf,"%6ld",temp);
    blink("ZREF 0", ascbuf, 10);
    getch(0);

    n = DecimalPointNum(disform);
    switch(n){
        case 0: l = fac2float(fac0-0X3000000) * 1000; break;
        case 1: l = fac2float(fac0-0X3000000) * 100 ; break;
        case 2: l = fac2float(fac0-0X3000000) * 10  ; break;
        case 3: l = fac2float(fac0-0X3000000)       ; break;
        case 4: l = fac2float(fac0-0X3000000) / 10  ; break;
        case 5: l = fac2float(fac0-0X3000000) / 100 ; break;
    }
    if (full1) f = (l / ((float)full1 / (float)(fpow(10,n)*10)));
    else       f =  l;
    sprintf(ascbuf,"%7.5f",f);
    blink("FACTOR", ascbuf, 10);
    while(!fkey){
        watchdog();
        bcdbuf = bcd(0 , 2);
    }
    fkey = 0;
}

void show_bcd()
{
    xdata ulong f, m, x;
    xdata uchar d;

    d = 131; // 0.000 //disform;
    if (!opvol){
        f = 20;
        m = 4;
    }else{
        f = 5;
        m = 0;
    }
    f *= fpow(10 , DecimalPointNum(d));
    m *= fpow(10 , DecimalPointNum(d)); 
    fngz = 0;
    if (!opbcd){
        x = (uint)(((float)0 / (float)(f)) * 0x0fff);
        bcdbuf = x;
        if (!opvol) x = 4000;
        else        x = 0;
    }
    else{
        x = 0;
        bcdbuf = bcd(0, 0);
    }
    do{
        if (opvol){
            x = getdata("\n0 - 5 ", x, d, 0);
//sprintf(ascbuf,"\n%ld",x); comstring(ascbuf); while(fsend);
        }else{
            if (!opbcd) x = getdata("\n4 - 20", x, d, f);
            else        x = getdata(" \nBCD  ", x, disform, 0);
        }
        if (!opbcd){
           if (x < m && x) x = m;
           bcdbuf = (uint)(((float)(x-m) / (float)(f-m)) * 0x0fff);
     }else bcdbuf = bcd(x, 0);
        watchdog();
    }while(x);
}


bit  CheckIfOpOn(uchar, ulong);
void check_option()
{
    xdata uchar p[4], i;
    xdata ulong l,j,l1,j1;

    if ((!opset)||(opset>51)) opset = 1 ;

    j = (ulong)novop;
    memcpy(p,opdef+(opset-1)*8,4);
    l = ConvertLongByte(p);

    j1 = (ulong)novop2;
    memcpy(p,opdef+((opset-1)*8)+4,4);
    l1 = ConvertLongByte(p);

    printf("\nS.OP-%02d",(uint)opset); 
    getch(0);
    if ((j != l) || (j1 != l1)){
        for (i=0; i<opnum; i++){
            if (i < 32){
                if (CheckIfOpOn(i, j) != CheckIfOpOn(i, l)){
                    printf("\nOP.%02u-%1u",(uint)i+1,(uint)CheckIfOpOn(i, j));  
                    getch(0);
                }
            }else{
                if (CheckIfOpOn(i-32, j1) != CheckIfOpOn(i-32, l1)){
                    printf("\nOP.%02u-%1u",(uint)i+1,(uint)CheckIfOpOn(i-32, j1)); 
                    getch(0);
                }
            }

        }
    }
    pdone();
}

bit CheckIfOpOn(uchar i, ulong x)
{
    temp = 1;
    temp <<= i;
    if (x & temp) return 1;
    else          return 0;
}

void show_weight()
{
    adcom(0, adraw);
    progvar = 0;
    while (!fkey){
        watchdog();
        if (atod(0)){
            atod2weight(brut, 0);
        }
    }
    getch(0); 
    adcom(0, adwey);
}

void remove_weight()
{
    uchar c,f=0;

    adcom(0, adraw);
    while (!f){
        watchdog();
        if (atod(0)){
            temp = zref0;
            c = hibyte(zref0);
            temp >>= 8;
            hiword(temp) = 0;
            temp = (ulong)fan2gul(brut);
            temp += 32767;
            zref0 = temp << 8;
            hibyte(zref0) = c;
            f = 1;
        }
    }
    adcom(0, adwey);
}

void force_zero()
{
//    if (hibyte(weight)) return;
    if (!mforce || !tforce0 /*|| fngz*/) return;
    if (dpcs || treqz || fone) return;

    temp = labs(weight);

    if ((temp + wtare(tare)) <= (ulong)mforce && !tstab && temp && !fover){
        if (!tforce) treqz = 30;
//sprintf(ascbuf,"\n1 %ld %ld %ld",temp,(ulong)mforce,(temp + wtare(tare))); comstring(ascbuf); while(fsend);
    }else{
        tforce = tforce0;
//sprintf(ascbuf,"\n2 %ld %ld %ld",temp,(ulong)mforce,(temp + wtare(tare))); comstring(ascbuf); while(fsend);
    }
}

void PowerOffm()
{
    xdata uchar k;

    k = poff;
    if (k > 99) k = 0;
    k = getdata("\nP-OFF ",(ulong)k, 0, 99);
    
    if (ktare){
        if (k != poff){
            pdone();
            poff = k;
            novsave();
            pause(5);
        }
    }
    if (kprint);
}

void    ZeroPrint_function(void)
{
    xdata uchar f=0;
    xdata ulong x;

    x = getdata(" 2ERO ", zerop, disform, 32000);
    if (zerop != x) f=1;
    zerop = x;
 
    if (f) novsave();
    pdone() ;
}