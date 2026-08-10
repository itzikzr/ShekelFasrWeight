#include "fanbase.h"

//  Try to keep lenght equality of all titles
//      36 signs it is width of usual label
//  27 signs it is width of termo label
//
//                  1...5...10...15...20...25...30...35.
//                    "----------------------------------------"
#define title1      "        S H E K E L    S C A L E        "
#define title2      "                                        "

#define title3      "           ל ק ש    י נ ז א מ           "
#define title4      "                                        "

#define title11     "   S H E K E L    S C A L E    "
#define title22     "                               "

#define title33     "      ל ק ש    י נ ז א מ       "
#define title44     "                               "


#define lstart  6
#define no      "|      |"
#define hkg     "    גק |"
#define ekg     " KG    |"
#define eorder  "| NUM. |"
#define horder  "|  .סמ |"
#define lnetto  8
#define enetto  " NETTO |"
#define hnetto  "   וטנ |"
#define lgross  8
#define egross  " GROSS |"
#define hgross  " וטורב |" 
#define ltare   8
#define etare   "  TARE |"
#define htare   "   הרט |"
#define ltotal  14
#define etotal  " TOTAL |"
#define htotal  "  כ\"הס |"//"
#define line    "--------"
#define row     "|"
#define etime   "  TIME  |"
#define htime   "  העש   |"
#define ldate   8
#define edate   "  DATE  "
#define hdate   " ךיראת  " 

//void  advance(void) ;                         // move ahead for 'tear-off' facility
void  reverse(void) ;                         // move back for 'tear-off' facility
void  labwgt(void);
void  TotalPrint();
void  LinePrint();
void  title();
void  PrintWeight(ulong);

void lprintf(uchar *s)
{
    xdata uchar t = 0, b;

    if ((otonge) && (!oldheb)){             // 1 - new, 0 - old hebrew table
        while(s[t]){
            watchdog();
            if ((s[t] >= 'א')&&(s[t] <= 'ת'))  b = s[t++] - 0x80 ;
            else                               b = s[t++];
            sprintf(ascbuf,"%c",b);
            comstring(ascbuf); while(fsend) watchdog();
        }
    }else{
        while(s[t]){
            watchdog();
            if ((s[t] >= 'א')&&(s[t] <= 'ת'))  b = s[t++] - 0x60 ;
            else                               b = s[t++];
            sprintf(ascbuf,"%c",b);
            comstring(ascbuf); while(fsend) watchdog();
        }
    }
}

void    eol(void)
{
    uchar  n ;
    uchar  crlf[] = { 0x0d, 0x0a } ;

    n = (olfeed) ? 1 : 2 ;                  // 1 - CR without LF for CITIZEN
    binstring(crlf, n) ;
}
void    eop(void) ;
void    eop(void)                               // end of label's page
{
    int n ;

    n = (int)line0 - 12 ;
    if (otear) n += tear ;
    watchdog() ;
    while(n-- > 0) eol() ;
}
void Print(uchar n)
{
    if (funder || fover) return;
    puts("\nPRINT ");
    timeout = 8;
    switch(n) {
        case 1 : TotalPrint(); break;
        case 2 : if (olabel) labwgt();
                 else        LinePrint();  
    }
    while(timeout) watchdog();
    tforce = tforce0;
}

void Total()
{
    if (!fzero || opPrint){
        if ((funder || fover || (0 > brut) || (brut > 999999)) && !opPrint) brut = 0;
        order  += 1 ;
        netto   = (brut - szero);//brut;
        gtotal  = (brut - szero) + wtare(tare);
        ptotal += netto;
    }
    frelsub8 = 1;
}

void title()
{
    xdata uchar i = 0, j;
    xdata uchar date[9], time[9] ;
    xdata uchar l[40];

    if (gettime()){
        sprintf(date, "%02d.%02d.%02d", (uint)sdate.da_day,(uint)sdate.da_mon,(uint)sdate.da_year%100) ;
        sprintf(time, "%02d:%02d:%02d", (uint)stime.ti_hour,(uint)stime.ti_min,(uint)stime.ti_sec);
    }else{
        sprintf(date, "11.11.11") ;
        sprintf(time, "00.00.00") ;
    }

    if (ogross) i++;
    if (ototal) i++;
    if (otare)  i++;
    i += 2;               // order + netto

    if (otonge){
        if (olabel && otear) reverse() ;
        if(!onotitle){
            if (!ollabel){
                lprintf(title3) ; eol() ;
                lprintf(title4) ; eol() ;
            }else{
                lprintf(title33) ; eol() ;
                lprintf(title44) ; eol() ;
            }
        }
        if (!olabel){
            sprintf(secbuf, "%21s%s", date, " :ךיראת");
            lprintf(secbuf) ; eol() ;
            sprintf(l, "%21s%s", time, " :  העש") ;
            lprintf(l) ; eol() ; eol() ;
    
            for(j=0;j<i;j++) lprintf(line); eol();
            lprintf(horder);
            lprintf(hnetto);
            if (ogross) lprintf(hgross);
            if (otare ) lprintf(htare );
            if (ototal) lprintf(htotal);
            eol();
            lprintf(no);
            for(j=0;j<i-1;j++) lprintf(hkg );  eol();
            for(j=0;j<i  ;j++) lprintf(line);  eol();
        }else{
            if (!ollabel)
                sprintf(secbuf, "%s%s    %s%s", date, "  : ךיראת", time, "  : העש");
            else
                sprintf(secbuf, "%s%s %s%s", date, " :ךיראת", time, " :העש");
            lprintf(secbuf) ; eol() ; eol();
        }
    }
    else{
        if(!onotitle){
            if (!ollabel){
                lprintf(title1) ; eol() ;
                lprintf(title2) ; eol() ;
            }else{
                lprintf(title11) ; eol() ;
                lprintf(title22) ; eol() ;
            }
        }
        if (!olabel){
            sprintf(l, "%20s%s", "DATE : ", date) ;
            lprintf(l) ; eol() ;
            sprintf(l, "%20s%s", "TIME : ", time) ;
            lprintf(l) ; eol() ; eol() ; 
            for(j=0;j<i;j++) lprintf(line); eol();
            lprintf(eorder);
            lprintf(enetto);
            if (ogross) lprintf(egross);
            if (otare)  lprintf(etare);
            if (ototal) lprintf(etotal);
            eol();
            lprintf(no);
            for(j=0;j<i-1;j++) lprintf(ekg);    eol();
            for(j=0;j<i;j++) lprintf(line);     eol();
        }else{
            if (!ollabel)
                sprintf(secbuf, "DATE :  %s    TIME :  %s", date, time) ;
            else
                sprintf(secbuf, "DATE: %s TIME: %s", date, time) ;
            lprintf(secbuf) ; eol() ; eol() ;
        }
    }
}

void LinePrint()
{
    xdata uchar l[10];

    if (ftitle) title();
    if (fnew) Total();
    fnew = ftitle = 0;
    sprintf(l, "%7d", order); lprintf(l); 
    lprintf(" "); 
    PrintWeight(netto);
    if (ogross) {lprintf(" "); PrintWeight(gtotal);}
    if (otare ) {lprintf(" "); PrintWeight(wtare(tare));}
    if (ototal) {lprintf(" "); PrintWeight(ptotal);}
    eol();
}

void  TotalPrint()
{
    xdata uchar l[10];

    eol();eol();eol();eol();eol();
    if (otonge){
        if(!onotitle){
            lprintf(title3) ; eol() ;
            lprintf(title4) ; eol() ;
        }
        lprintf(line); lprintf(line); eol();
        lprintf(horder);
        lprintf(htotal);
        eol();
        lprintf(no);
        lprintf(hkg);   eol();
        lprintf(line); lprintf(line); eol();
    }
    else{
        if(!onotitle){
            lprintf(title1) ; eol() ;
            lprintf(title2) ; eol() ;
        }
        lprintf(line); lprintf(line); eol();
        lprintf(eorder);
        lprintf(etotal);
        eol();
        lprintf(no);
        lprintf(ekg);   eol();
        lprintf(line); lprintf(line); eol();
    }
    sprintf(l, "%7d", order); lprintf(l); 
    lprintf(" ");
    PrintWeight(ptotal);
    eol();
    ftitle = 1;
    if (!ototnz) ptotal = gtotal = order = netto = 0;
    netto = brut;
eop();
//    order = ptotal = netto = gtotal = 0;
}

void  PrintWeight(ulong n)
{
    xdata uchar m,l[10];

    m = DecimalPointNum(disform);
		if (!(hibyte(n) & 0x80) || !opPrint)
		{
//sprintf(ascbuf,"\n1 %ld  %7.2f \n", n, -1*(((float) abs(n))*0.1)); comstring(ascbuf); while(fsend);								
			switch(m)
			{
					case 5 : sprintf(l,"%7.5f", ((float) n) * 0.00001); break; // 0.00000
					case 4 : sprintf(l,"%7.4f", ((float) n) * 0.0001);  break; // 0.0000
					case 3 : sprintf(l,"%7.3f", ((float) n) * 0.001);   break; // 0.000
					case 2 : sprintf(l,"%7.2f", ((float) n) * 0.01);    break; // 0.00
					case 1 : sprintf(l,"%7.1f", ((float) n) * 0.1);     break; // 0.0
					case 0 : sprintf(l,"%7ld" ,          n)      ;      break; // 0
			}
		}
		else
		{
			switch(m)
			{
					case 5 : sprintf(l,"%7.5f", -1 * (((float) abs(n)) * 0.00001)); break; // 0.00000
					case 4 : sprintf(l,"%7.4f", -1 * (((float) abs(n)) * 0.0001));  break; // 0.0000
					case 3 : sprintf(l,"%7.3f", -1 * (((float) abs(n)) * 0.001));   break; // 0.000
					case 2 : sprintf(l,"%7.2f", -1 * (((float) abs(n)) * 0.01));    break; // 0.00
					case 1 : sprintf(l,"%7.1f", -1 * (((float) abs(n)) * 0.1));     break; // 0.0
					case 0 : sprintf(l,"%7ld" , -1 *           abs(n))       ;      break; // 0
			}
//sprintf(ascbuf,"\n2 %ld  %7.2f \n", n, -1*(((float) abs(n))*0.1)); comstring(ascbuf); while(fsend);								
		}
    lprintf(l);
}

void    subdisp(void)
{
    printf("\n TOTAL") ; pause(onesec);
    display(ptotal,disform) ;
    pause(twosec) ;
    printf("\n COUNT"); pause(onesec) ;
    sprintf(ascbuf, "\n%6d", order) ;
    printf(ascbuf) ;    pause(twosec) ;
}

void    clear_all(void)
{
    printf("\nCLEAR ");
    twait = twosec;
    while(twait){
        bcdbuf = bcd(0 , 2);
//        if (ktare && kbhit())
        {
            if (cdouble('T')){
                order  = gtotal = ptotal = netto = 0;
                ftitle = 1;
                pdone() ;
            }
        }
        watchdog();
    }
}

void    pdone(void)
{
    printf ("\n DONE ") ;
    pause(onesec) ;
}

//      label format for weighing
//      -------------------------



void    reverse(void)
{
    uchar n = tear ;

    while(n > 2)
    {
        lprintf("jl") ; n -= 3 ;           // esc,j,108 - 3 LReverse
    }
    if (n == 2)  lprintf("j6") ; else      // esc,j,54  - 2 LReverse
        if (n == 1)  lprintf("j'") ; else      // esc,j,39  - 1 LReverse
    /* no print */ ;
}

void    labwgt(void)
{
    xdata uchar   dot[2][37] =
    {
        "------------------------------------",
        "|                                  |"
    } ;
    xdata uchar   dot1[2][37] =
    {
        "-----------------------------",
        "|                          |"
    } ;
    xdata float   d1, d2, d3;
    xdata uchar   m, s[40];

    m = DecimalPointNum(disform);
    title() ;
    if (fnew) Total();
    fnew = 0;
    d1  = (float)labs(wtare(tare) + netto) / fpow(10, m) ;
    d2  = (float)labs(wtare(tare))         / fpow(10, m) ;
    d3  = (float)labs(netto)               / fpow(10, m) ;
				
		if (!(hibyte(netto) & 0x80) || !opPrint);
		else	d3 *= (-1);

		if (!(hibyte(netto) & 0x80) || !opPrint);
		else			d1 = d3 + d2;
		
    if (otonge){
        if (!ollabel) sprintf(s, "%28d  : רפסמ", order); 
        else          sprintf(s, "%21d  : רפסמ", order); 
        lprintf(s) ; eol() ; eol() ;
        if (!ollabel) sprintf(s, "%8.*f  : וטורב",(uint)m,d1);
        else          sprintf(s, "%8.*f :וטורב",(uint)m,d1);
        lprintf(s) ;
        if (!ollabel) sprintf(s, "    %8.*f  : הרט",(uint)m, d2);
        else          sprintf(s, " %8.*f :הרט",(uint)m, d2);
        lprintf(s) ; eol() ;
        if (!ollabel) sprintf(s, "|%c       וטנ       %c|", 0x0e, (olfeed) ? 0x1f : 0x14) ;
        else          sprintf(s, "|%c     וטנ     %c|", 0x0e, (olfeed) ? 0x1f : 0x14) ;
        if (!ollabel) sprintf(secbuf, "|%c  גק   %8.*f  %c|", 0x0e, (uint)m, d3, (olfeed) ? 0x1f : 0x14) ;
        else          sprintf(secbuf, "|%c גק %8.*f %c|", 0x0e, (uint)m, d3, (olfeed) ? 0x1f : 0x14) ;

    }else{
        sprintf(s, "NUM. :  %8d", order) ;
        lprintf(s) ; eol() ; eol() ;
        if (!ollabel) sprintf(s, "TARE :  %8.*f    ",(uint)m, d2) ;
        else          sprintf(s, "TARE: %7.*f  ",(uint)m, d2) ;
        lprintf(s) ;
        if (!ollabel) sprintf(s, "GROSS:  %8.*f",(uint)m, d1) ;
        else          sprintf(s, "GROSS: %7.*f",(uint)m, d1) ;
        lprintf(s) ; eol() ;
        if (!ollabel) sprintf(s, "|%c      NETTO      %c|", 0x0e,(olfeed) ? 0x1f : 0x14) ;
        else          sprintf(s, "|%c    NETTO    %c|", 0x0e,(olfeed) ? 0x1f : 0x14) ;
        if (!ollabel) sprintf(secbuf, "|%c  %8.*f   KG  %c|",0x0e, (uint)m, d3, (olfeed) ? 0x1f : 0x14) ;
        else          sprintf(secbuf, "|%c %8.*f KG %c|",0x0e, (uint)m, d3, (olfeed) ? 0x1f : 0x14) ;
    }
    if (!ollabel){
        lprintf(dot[0]) ; eol() ;
        lprintf(s)      ; eol() ;
        lprintf(dot[1]) ; eol() ;
        lprintf(secbuf) ; eol() ;
        lprintf(dot[1]) ; eol() ;
    }else{
        lprintf(dot1[0]) ; eol() ;
        lprintf(s)       ; eol() ;
        lprintf(dot1[1]) ; eol() ;
        lprintf(secbuf)  ; eol() ;
        lprintf(dot1[1]) ; eol() ;
    }
    eop() ;
}

//      label format for subweighing
//      ----------------------------
void    subwgt(void)
{
    xdata uchar   dot[2][37] =
    {
        "------------------------------------",
        "|                                  |"
    } ;
    xdata uchar   string[40],m ;
    xdata float   d1;

    title() ;
    m = DecimalPointNum(disform);
    d1  = (float)labs(ptotal) / fpow(10, m) ;
		
		if (!(hibyte(ptotal) & 0x80) || !opPrint);
		else d1 *= (-1);

    if (otonge){
        eol() ; 
        sprintf(secbuf, "%21d  : תוליקש כ\"הס", order) ; 
        lprintf(secbuf) ; eol() ; eol() ;
        sprintf(string, "|             וטנ כ\"הס             |") ;
        sprintf(secbuf, "|   %cגק  %10.*f%c   |", 0x0e, (uint)m, d1, (olfeed) ? 0x1f : 0x14) ;
    }else{
        eol() ; 
        sprintf(secbuf, "TOTAL WEIGHINGS :  %8d", order) ;
        lprintf(secbuf) ; eol() ; eol() ;
        sprintf(string, "|           TOTAL  NETTO           |") ;
        sprintf(secbuf, "|   %c%10.*f  KG%c   |",0x0e, (uint)m, d1, (olfeed) ? 0x1f : 0x14) ;
    }
    lprintf(dot[0]) ; eol() ;
    lprintf(string) ; eol() ;
    lprintf(dot[1]) ; eol() ;
    lprintf(secbuf) ; eol() ;
    lprintf(dot[1]) ; eol() ;
    eop() ;
    if (!ototnz) ptotal = gtotal = order = netto = 0;
}