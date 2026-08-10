#include "fanbase.h"

double fabs (double) ;
uchar  cs(uchar *st) ;

void   SendWeightToCom(ulong b)
{
    xdata uchar n, d, f=0;
    xdata float fdisp ;

    if (comstat()){
        n = getcom();
    }
    freqw = fnew = 0;
    n = DecimalPointNum(disform);
    if (dpcs) n = 0;

    if (errnum){
        sprintf(ascbuf, "ERROR-%02d\r",(uint)errnum);
        comstring(ascbuf); while(fsend) watchdog();
        return;
    }    

    if (opadi && !opseg && ospace)
        if (funder || fover){
            if (funder){f=1; sprintf(ascbuf, " L%c",cr);}
            if (fover ){f=1; sprintf(ascbuf, " H%c",cr);}
            if (f){comstring(ascbuf); return;}
        }

    if ((funder && oneg) || fover){
        if (opwiz){
            if (funder){f=1; sprintf(ascbuf, "%cL%c",cr, lf);}
            if (fover ){f=1; sprintf(ascbuf, "%cH%c",cr, lf);}
        }else{
            if (!ospace){
                if (funder){f=1; sprintf(ascbuf,"\nUNDER");}
                if (fover ){f=1; sprintf(ascbuf,"\nSTOP ");}
            }else{
                if (funder){f=1; sprintf(ascbuf,"\n UNDER");}
                if (fover ){f=1; sprintf(ascbuf,"\n STOP ");}
            }
        }
        if (f){comstring(ascbuf); return;}
    }

    if (labs(b) > 999999) return;

    if (funder || fngz || hibyte(b)) d = '-';
    else                             d = '+';
    if (dpcs) d = '+';

    if (!odisp){
        if (dpcs) fdisp = olddisp;
        else      fdisp = (float)labs(olddisp)/fpow(10,n) ;
    }else{
        if (dpcs) fdisp = b;
        else      fdisp = (float)labs(b)/fpow(10,n) ;
    }

    if (opwiz){
        if (opone){
           if (!ospace){
                sprintf(secbuf, "%06.*f", (uint)n, fabs(fdisp));
                if (opcs) sprintf(ascbuf, "%c%06.*f%c%c", cr, (uint)n, fabs(fdisp),cs(secbuf),lf);
                else      sprintf(ascbuf, "%c%06.*f%c", cr, (uint)n, fabs(fdisp),lf);
            }else{
                sprintf(secbuf, "%6.*f", (uint)n, fabs(fdisp));
                if (opcs) sprintf(ascbuf, "%c %6.*f%c%c", cr, (uint)n, fabs(fdisp),cs(secbuf),lf);
                else      sprintf(ascbuf, "%c %6.*f%c", cr, (uint)n, fabs(fdisp),lf);
            }
        }else{
            if (!ospace){
                sprintf(secbuf, "%c%07.*f", cr, (uint)n, fabs(fdisp));
                if (opcs) sprintf(ascbuf, "%c%07.*f%c%c", cr, (uint)n, fabs(fdisp),cs(secbuf),lf);
                else      sprintf(ascbuf, "%c%07.*f%c", cr, (uint)n, fabs(fdisp),lf);
            }else{
                sprintf(secbuf, "%c%7.*f", cr, (uint)n, fabs(fdisp));
                if (opcs) sprintf(ascbuf, "%c %7.*f%c%c", cr, (uint)n, fabs(fdisp),cs(secbuf),lf);
                else      sprintf(ascbuf, "%c %7.*f%c", cr, (uint)n, fabs(fdisp),lf);
            }
        }
    }
    else{
/*        
        if (!ospace){
            if (opseg){
                if (oastrk) sprintf(secbuf, "*%c%07.*f", d, (uint)n, fabs(fdisp)) ;
                else        sprintf(secbuf, "%c%07.*f" , d, (uint)n, fabs(fdisp)) ;
            }else{
                if (oastrk) sprintf(secbuf, "*%07.*f", (uint)n, fabs(fdisp)) ;
                else        sprintf(secbuf, "%07.*f" , (uint)n, fabs(fdisp)) ;
            }
        }else{
            if (opseg){
                if (oastrk) sprintf(secbuf, " *%c%07.*f", d, (uint)n, fabs(fdisp)) ;
                else        sprintf(secbuf, " %c%07.*f" , d, (uint)n, fabs(fdisp)) ;
            }else{
                if (oastrk) sprintf(secbuf, " *%07.*f", (uint)n, fabs(fdisp)) ;
                else        sprintf(secbuf, " %07.*f" , (uint)n, fabs(fdisp)) ;
            } 
        }
*/        
        sprintf(secbuf, "%07.*f" , (uint)n, fabs(fdisp)) ;
        
        if (opseg){
            sprintf(ascbuf, "%c%s",d ,secbuf);
            sprintf(secbuf, "%s", ascbuf);
        }
        
        if (oastrk){
            sprintf(ascbuf, "*%s",secbuf);
            sprintf(secbuf, "%s", ascbuf);
        }
        
        if (ospace){
            sprintf(ascbuf, " %s",secbuf);
            sprintf(secbuf, "%s", ascbuf);
        }
        
        if (opprints){
            sprintf(ascbuf, "%c%s",(fstab)? 'S': 'U' ,secbuf);
            sprintf(secbuf, "%s", ascbuf);
        }

        if (opcs) sprintf(ascbuf, "%s%c\r",secbuf, cs(secbuf));
        else      sprintf(ascbuf, "%s\r",secbuf);
    }
    if (!fsend){
        comstring(ascbuf); 
        while(fsend) watchdog();
    }
}

float fpow(float b, int e) // expanenta calculation
{
    xdata float  bas = 1 ;

    if (e < 0) while(e++) bas /= b ;
    else       while(e--) bas *= b ;
    return(bas) ;
}

void serial()
{
    xdata uchar k, f=0;

    k = getcom();

    ascbuf[0] = 0;
    if (onet){
        if (phext && (!trsnet)) {
            phext = await = 0 ;
        }    
        network(k);
    }else{
		if (k != 't' && k != 'T')
        	k = toupper(k);
        if (oprofi){
            if (!fStartProfi) return;
            if (k != 'R' && fpatod){
                fpatod = 0;
                adcom(0, adwey);
            }
            switch(k){
                case 'J' : sendprofiID()     ; break ; // profibus ID
                case 'A' : sendaddress()     ; break ; // profibus address
                case 'W' : sendweight()      ; break ; // weight
                case 'I' : sendidentifier()  ; break ; // IDENTIFIRE
                case 'R' : sendatod()        ; break ; // ATOD
                case 'F' : sendfullscale()   ; break ; // FULL SCALE
                case 'V' : sendtarevale()    ; break ; // VALUE OF TARE
                case 'D' : senddecimalpoint(); break ; // DECIMAL POINT
                case 'Z' : sendzero()        ;         // zero
                           treqz  = trisec; 
                           ftzero = 1;
                           treqz  = twosec;    break;
                case 'T' : sendtare()        ;         // tare
                           treqt = twosec    ; break;
                case 'S' : resersacle()      ; break ; // RESET
            }
            
        }else{
			if (opcasio){
				comuniwell(k);
			}else{
	            switch (k){
	                case 'N': examine(novbuf); break;
	                case 'W': freqw = f = 1;                     break; 
	                case 'T': if (!fngz){treqt = twosec; ftranstare = f = 1;} break;
	                case 'Z': treqz = trisec; f = ftzero = 1;    break;
	                case 'I': if (opwiz){SendIdent(); f = 1;}    break;
					case 't': SendWeightToCom(wtare(tare)); f = 1; break;
	            }
			}
        }
    }
}

uchar DecimalPointNum(uchar k)
{
    switch(k)
    {
        case 129: return 5; // 0.00000
        case 1  : return 5; //  .00000
        case 130: return 4; //  0.0000
        case 2  : return 4; //   .0000
        case 131: return 3; //   0.000
        case 3  : return 3; //    .000
        case 132: return 2; //    0.00
        case 4  : return 2; //     .00
        case 133: return 1; //     0.0
        case 5  : return 1; //      .0
        case 0  : return 0; //       0
    }
		return 3;
}

uchar   cs(uchar *s)
{
    xdata uchar  ret = 0, i=8;

    while(i)
    {
        if ((*s >= '0')&&(*s <= '9')) ret += *s-0x30 ;
        s ++ ;
        i--;
    }
    ret %= 10 ;
    if (!ret) return('0') ;
    else return(0x3a-ret) ;
}

void  SendIdent()
{
    sprintf(ascbuf, "%c%5u%c",cr, vernum, lf) ;
    comstring(ascbuf); while(fsend) watchdog();
}

void SendZeroReq(uchar i)
{
    if (i) sprintf(ascbuf, "%cZ%c",cr, lf) ;
    else   sprintf(ascbuf, "%cF%c",cr, lf) ;
    ftzero = 0;
    comstring(ascbuf); while(fsend) watchdog();
}

void xputs()
{
    uchar   sta=0;
    float   fdisp;

    if (dzero) sta |= 0x01;
    if (dtare) sta |= 0x02;
    if (dkg  ) sta |= 0x04;
    if (dlb  ) sta |= 0x08;
    if (dbat ) sta |= 0x20;
    if (dpcs ) sta |= 0x40;

    if (!fover && (!funder && !(weight0 < szero) || !oneg)){
        fdisp = (float)labs(weight)/fpow(10,DecimalPointNum(disform)) ;
        if (fngz || hibyte(weight)) fdisp *= -1;
        if(dpcs || !DecimalPointNum(disform)) 
            sprintf(secbuf, "\n%c%6ld",sta,(ulong)brut) ;        
        else
            sprintf(secbuf, "\n%c%7.*f",sta, (uint)DecimalPointNum(disform), fdisp) ;
    }else{
        if (fover ) sprintf(secbuf, "\n%c%s",sta," STOP ");
        else        sprintf(secbuf, "\n%c%s",sta,"------");
    }
    if (!fsend) comstring(secbuf); while(fsend) watchdog();
}


////////////////////    profi bus ////////////////////////////////

#define byte(n, m)      *(((uchar *) (&n)) + m)

void sendweight()
{
    uchar s=0x08, sum, c1, c2;
    uint  w;
    float fdisp ;

    fdisp = (float)brut/fpow(10, DecimalPointNum(disform)) ;
    if (fdisp < 0) s |= 0x06;

    if (funder || (weight0 < szero)) s |= 0x02; // under
    if (fover) s |= 0x04; // overload

    if (!fstab ) s |= 0x01;
    if (fstatus && oldprofi) s |= 0x80;

    if (brut >= 65500){
        w = 65500;
        s |= 0x04;
    }
    else w = (uint)brut;

    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = s + c1 + c2;
    sprintf(secbuf,"%c%c%c%c",s, c1, c2, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void sendidentifier()
{
    uchar sum, c1, c2;
    uint  w;

    fstatus = 0;
    if (odfrofi && oldprofi) w = (uint)vernum;
    else                     w = 0;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'I' + c1 + c2;
    sprintf(secbuf,"I%c%c%c", c1, c2, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void sendatod()
{
    uchar sum, c1, c2,f=1;
    uint  w;

    if (!fpatod){   
        adcom(0, adraw);
        fpatod = 1;
        while(f){
            watchdog();
            if (atod(0)) f = 0;
        }
    }
    w = fan2gul(brut);
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'R' + c1 + c2;
    sprintf(secbuf,"R%c%c%c", c1, c2, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void sendfullscale()
{
    uchar sum, c1, c2;
    uint  w;

    w = (uint)wstop;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'F' + c1 + c2;
    sprintf(secbuf,"F%c%c%c", c1, c2, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void sendtarevale()
{
    uchar sum, c1, c2;
    uint  w;

    w = wtare(tare);
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'V' + c1 + c2;
    sprintf(secbuf,"V%c%c%c", c1, c2, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void resersacle()
{
    uchar sum;

    sum = 'S';
    sprintf(secbuf,"S%c%c%c", 0, 0, sum);
    binstring(secbuf,4); while(fsend) watchdog();
    reboot();
}

void sendzero()
{
    uchar sum;

    sum = 'Z';
    sprintf(secbuf,"Z%c%c%c", 0, 0, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void sendtare()
{
    uchar sum;

    sum = 'T' ;
    sprintf(secbuf,"T%c%c%c", 0, 0, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void senddecimalpoint()
{
    uchar sum, c1, c2;
    uint  w;

    w = (uint)DecimalPointNum(disform);
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'D' + c1 + c2;
    sprintf(secbuf,"D%c%c%c", c1, c2, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void sendaddress()
{
    uchar sum, c1, c2;
    uint  w;

    w = (uint)profiadd;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'A' + c1 + c2;
    sprintf(secbuf,"A%c%c%c", c1, c2, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void sendprofiID()
{
    uchar sum, c1, c2;
    uint  w;

    w = (uint)profiID;
    c1 = byte(w , 0);
    c2 = byte(w , 1);
    sum = 'J' + c1 + c2;
    sprintf(secbuf,"J%c%c%c", c1, c2, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void senderror(uchar n)
{
    uchar sum;

    fStartProfi = 0;
    sum = 'E' + n;
    sprintf(secbuf,"E%c%c%c", 0, n, sum);
    binstring(secbuf,4); while(fsend) watchdog();
}

void comuniwell(uchar ret)
{
    xdata uchar i, cs=0 , d, ch=0;

    if(ret == 0x7){
        timeout = 10;
        do{
            if(comstat()) ch = getcom();
            watchdog();
        }
        while(ch!=0x7 && timeout);
        if (fstab) d = 0x0;
        else       d = 0x1;
        //if((fextra && !fneg) || fng){
		if (funder || fngz || hibyte(brut) || fover){
			d	   = 0x01;
			weight = 0;
		}
        sprintf(ascbuf,"%05ld", brut);
        secbuf[0] = 8;
        secbuf[1] = 0;
        secbuf[2] = ascbuf[0]-0x30;
        secbuf[3] = ascbuf[1]-0x30;
        secbuf[4] = ascbuf[2]-0x30;
        secbuf[5] = ascbuf[3]-0x30;
        secbuf[6] = ascbuf[4]-0x30;
        for(i=7;i<18;i++) secbuf[i] = (uchar)0x0;
        secbuf[18] = d;
        for(i=0;i<18;i++) cs += secbuf[i];
        secbuf[19] = cs ;
        binstring(secbuf,20);
    }
}