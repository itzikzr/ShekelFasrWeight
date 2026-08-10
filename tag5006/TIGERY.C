#include "tiger.h"

uchar  *convert_long_weight_to_float(long);

void    getname(void)
{
    clrscr()  ;
    flag(fmidl, 1) ; flag(fmidr, 1) ;
    printf("\n NAME ") ;
    clrlcd(); 
    lcdputs(1,1,acode);
    ///fread(acode) ;
    if (aname != bigname(" NAME ", aname , 1 ,otonge))
    {
        strcpy(aname, ascbuf1);
        if (!is_space(aname))  fwrite(acode) ;
        //strcpy(aname, ascbuf1) ; fread(acode) ;
        defpour(); lcddone(); 
        pdone() ;
    }
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}

uchar  *bigname(uchar *m, uchar *sname, bit let, bit leng)
{
    uchar   ret, t, *p=NULL, asc[15], asc1[15];
    uchar   s[10], f='0', ind=0;
    uchar   name[17], pos=1;
    uint    x ,nkey ,n=0;
///    uchar   leter[] ={'A','B','C','1','D','E','F','2','G','H','I','3','J','K','L','4','M','N','O','5','P','Q','R','6','S','T','U','7','V','W','X','8','Y','Z',32,'9',0};
///    uchar   hleter[]={'Ä','Å','Ç','1','É','Ñ','Ö','2','Ü','á','à','3','â','ã','å','4','é','ê','ë','5','í','î','ñ','6','ó','ò','ô','7','ö','ä','è','8','ç','ì','ï',32,'9',0}; 
    uchar   leter[] ={' ','(',')','0',  'A','B','C','1',
                      'D','E','F','2',  'G','H','I','3',
                      'J','K','L','4',  'M','N','O','5',
                      'P','Q','R','6',  'S','T','U','7',
                      'V','W','X','8',
                      'Y','Z',32,'9',0};
    uchar   hleter[]={' ',')','(','0',  'Ä','Å','Ç','1',
                      'É','Ñ','Ö','2',  'Ü','á','à','3',
                      'â','ã','å','4',  'é','ê','ë','5',
                      'í','î','ñ','6',  'ó','ò','ô','7',
                      'ö','ä','è','8',  'ç','ì','ï','9',0}; 

    bit     fleter = let ;      // for letter input
    bit     fclear = 0 ;        // for <C> key press
    bit     fstart = 0 ;        // for any key press

    asc[0] = ascbuf1[0] = NULL;
    tlatters = onesec;
    tblink = twosec ; 
    n = strlen(sname);
    strcpy(ascbuf1,sname);

    clrlcd2(); 
    if (strcmp(sname,""))
        if(!leng) lcdputs(1,2,sname);
        else      lcdputs(1,2,htrans_new(sname));
    if (!strcmp(m," NAME ")){
        memcpy(mb, acode, 8);
        sprintf(mb,"CODE  %s",acode);
        lcdputs(1,1,mb);
    }
    else{
        if (!ocheck){ 
            sprintf(mb,"S.POINT NAME  %02d", (uint)spnameind+1);
            lcdputs(1,1,mb);
        }
    }

    while(true)
    {
        ret = kbhit() ? getch() : 0 ;
        if(fleter && (!tlatters && fstart==1)){
            ret = 'Z';
            fstart = 0;
            tlatters = onesec;
        }
        watchdog();
        switch(ret) {
            case 'Z' :
                if (n < 14) {
                    if (fstart == 1){
                        fstart = 0;
                        tlatters = onesec;                        
                        n++;
                    }
                }
                break;

            case 'C' : if (n){
                            memcpy(asc,ascbuf1+1,n);    
                            clrlcd2();
                            sprintf(ascbuf1,"%s",asc);  
                            if(!leng) lcdputs(1,2,ascbuf1);
                            else      lcdputs(1,2,htrans_new(ascbuf1));                    
                            n--;
                       }
                       if(cdouble('C')) {
                            asc[0] = ascbuf1[0] = f = n = 0;
                            clrlcd2();
                       }         
                       break;

            case 'F' : fscroll = false ;
                       ascbuf1[strlen(ascbuf1)+1]=NULL;
                       return ascbuf1;

            case 'T' : clrscr(); return sname;
    
            case '0': case '1': case '2': case '3': case '4': 
            case '5': case '6': case '7': case '8': case '9': 
            if (fleter) {
                //if (ret != '0') 
                { 
                    if(nkey != (ret-48) || !fstart) { 
                        sprintf (asc,"%s",ascbuf1);
                        if(!leng) sprintf(asc1,"%s-",ascbuf1);
                        else      sprintf(asc1,"-%s",ascbuf1);
                        clrlcd2();
                        if(!leng) lcdputs(1,2,asc1);
                        else      lcdputs(1,2,htrans_new(asc1));
                        fstart = 0;
                        tlatters = onesec;
                        n++;
                    }
                    x = (ret - f);
                    if (x == 0) ind++; else ind = 0;
                    if (ind > 3) ind = 0;
                    f = ret;
                    nkey = ret-48;
                    //if(!leng) ret =  leter[((ret-48)*4)-(4-ind)] ;
                    //else      ret = hleter[((ret-48)*4)-(4-ind)] ;
                    if(!leng) ret =  leter[ind + (4 * (ret-48))] ;
                    else      ret = hleter[ind + (4 * (ret-48))] ;
                }
            }
            else {
                ind = 0;
                f = ret;
                sprintf (asc,"%s",ascbuf1);
                sprintf (asc1,"%s-",ascbuf1);
                clrlcd2();
                if(!leng)lcdputs(1,2,asc1);
                lcdputs(1,2,htrans_new(asc1));
                fstart = 0;
                n++;
            }
            clrlcd2();
            if (strlen(ascbuf1) < 14){
                if(!leng) sprintf(ascbuf1,"%s%c",asc,ret);
                else      sprintf(ascbuf1,"%c%s",ret,asc);
                if(!leng) lcdputs(1,2,ascbuf1);
                else      lcdputs(1,2,htrans_new(ascbuf1));
            }else{
                memcpy(mb,asc,strlen(asc)-1);
                strcpy(asc,mb);
                if(!leng) sprintf(ascbuf1,"%s%c",mb,ret);
                else      sprintf(ascbuf1,"%c%s",ret,mb);
                if(!leng) lcdputs(1,2,ascbuf1);
                else      lcdputs(1,2,htrans_new(ascbuf1));                    
            }
            fstart = 1;
            tlatters = onesec;
            break;
        }
    }
}

void lcdstart1()
{
    heb_eng(1,1,"Ï˜˘","SHEKEL");
    heb_eng(1,2,"ı¯‡‰ ÈÊ‡Ó","Electronic Scale");
}
void lcddone()
{
    heb_eng(1,1,"ÚˆÂ·","DONE");
    clrlcd2();
}
void clrlcd()
{
    lcdputs(1, 1, "                 ");
    watchdog();
    lcdputs(1, 2, "                 ");
}
void clrlcd1()
{
    lcdputs(1, 1, "                 ");
}
void clrlcd2()
{
    lcdputs(1, 2, "                 ");
}
void define_cw_name(ulong m, uchar i)
{
    clrscr(); flag(fmidl, 1); flag(fmidr, 1);
    printf("\nCW.NAME"); clrlcd(); 
    lcdputs(1,1,convert_long_weight_to_float(m));     
    if (spname[i] != bigname("CW.NAME", spname[i],1,otonge))
    {
        strcpy(spname[i], ascbuf1);
        if (!is_space(aname))  fwrite(acode);
        defpour(); 
    }
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}

void define_sp_name(ulong m, uchar i)
{
    clrscr(); flag(fmidl, 1); flag(fmidr, 1);
    printf("\nSP.NAME"); clrlcd(); 
    lcdputs(1,1,convert_long_weight_to_float(m));     
    if (spname[i] != bigname("SP.NAME", spname[i],1,otonge))
    {
        strcpy(spname[i], ascbuf1);
        if (!is_space(aname))  fwrite(acode);
        defpour(); 
    }
    flag(fmidl, 0) ; flag(fmidr, 0) ;
}

uchar  *convert_long_weight_to_float(long n)
{
    switch (decim)
    {
        case 5 : sprintf(ascbuf,"%7.5f", ((float) n)*0.00001); break;
        case 4 : sprintf(ascbuf,"%7.4f", ((float) n)*0.0001); break;
        case 3 : sprintf(ascbuf,"%7.3f", ((float) n)*0.001); break;
        case 2 : sprintf(ascbuf,"%7.2f", ((float) n)*0.01); break;
        case 1 : sprintf(ascbuf,"%7.1f", ((float) n)*0.1); break;
        default: sprintf(ascbuf,"%6ld", n);
    }
    return ascbuf;
}

void lcd_weight()
{
    heb_eng(1,1,"‰ÏÈ˜˘","WEIGHT");
    clrlcd2();
}

uchar   *heb(uchar *s)           // hebrew translation
{
        code uchar lcdtab[] = {
        "˙˘¯˜ˆıÙÛÚÒÔÓÌÏÎÍÈËÁÊÂ‰„‚·‡"
        } ;
        uchar d[80], t, i = 0 ;

        while(s[i]) {
            if ((s[i] >='‡')&&(s[i] <='˙')) {
                for (t = 0 ; lcdtab[t] ; t++) {
                    if (s[i] == lcdtab[t]) {
                        if (t < 280){
                            d[i] = s[i] - 0x40;
                        }
                        else{
                            d[i] = t + 0xC0 ;
                        }
                        break ;
                    }
                    watchdog();
                }
            }
            else d[i] = s[i] ;
            i++ ;
            watchdog();
        }
        d[i] = 0 ;
//        sprintf(mb,"\n%s",d); comstring(mb); while(fsend);
        return d ;
}

void heb_eng(uchar i, uchar j, char *h, uchar *e)
{
    uchar l,m[16],n[16];

    if (j == 1) clrlcd1();
    if (j == 2) clrlcd2();

    mb[0] = m[0] = n[0] = 0;

    if(!otonge) strcpy(n , e);
    else        strcpy(n , h);

    if (strlen(n) < 15){
        l = 16 - strlen(n);
        l /= 2;    
        
        strspace(m ,l);
        strcat  (mb,m);
        strcat  (mb,n);
        strcat  (mb,m);
    }else strcpy(mb, n);    

    if(!otonge) lcdputs(i, j, mb);
    else        lcdputs(i, j, heb(mb));
//sprintf(mb,"\n%s",mb); comstring(mb); while(fsend);
}