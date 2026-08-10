#include "fanbase.h"

#define start           0x01
#define restart         0x02
#define stopkey         0x04

#define numw  5

#define byte(n, m)      *(((uchar *) (&n)) + m)

uchar   checksum(uchar, uchar) ;
void    sp_all_on();
void    sp_all_off();

void setpoint()
{
    uchar i;

    if ((brut == neww) && ((start & keystatus) || (restart & keystatus))){
        indw++;
        if (indw > numw) indw = numw;
    }else indw = 0;
    neww = 0;

//    if (!funder){

	if (optare && !fstart && (start & keystatus) && !ftare){
		ftare = 1;
		treqt = twosec;
	}

    if (fngz || funder || (weight0 < szero) || hibyte(brut)){
        sprel = 0;
    }else{
        if ((indw == numw) && ((!fstart && (start & keystatus) && (!brut || brut <= zerodiv)) || (restart & keystatus))){ // press start 
            fstart = fdelay1 = 1;
            ftare  = flock   = sprel = 0;
            if (ospord){
                for (i=0; i<spmax; i++) sprel |= (1 << i); // rel 1 to spmax on
            }else                       sprel  = 1;        // rel off 
        }
        if (stopkey & keystatus) ftare = fstart = sprel = 0; // press stop
        if (fstart && !flock){
            if (ospord) sp_all_on();
            else        sp_all_off();
        }
        if ((!brut || brut <= zerodiv)) sprel |= 0x0040; // rel 7 on
        else                            sprel &= 0xffbf; // rel 7 off
        if (!(sprel & 0x0080)) frelsub8 = 0;
    }

    send_rel(sprel);
}

void sp_all_on()
{
    xdata uchar t, m;
    xdata uint  s;
    xdata ulong max=0;

    if (fngz || funder || (weight0 < szero) || hibyte(brut)) return;
    
    for (t=0; t<spmax; t++) if (spdata[t]) m = t;
    for (t=0; t<spmax; t++) if (spdata[t] > max) max = spdata[t];
    for (t=0; t<=m; t++){
        if (spdata[t] && (spdata[t] <= brut)){
            s = 1; s <<= t; s =~ s;
            sprel &= s;
            if (spdata[t] == max){
                if (!frel8off){
                    frel8off = 1;
                    trel8off = dtime8;
                }
            }
        }else{
            sprel |= (1 << t);
            if (brut < max) frel8off = 0;
        }
    }

    if (!trel8off && frel8off){
        if (!frelsub8){
            if (osubrel8 &&  tstab) return;
            if (osubrel8 && !tstab) Total();
        }
        flock  = 1;
        fstart = 0;
        sprel  = 0x0080; // rel 8 on
    }
}

void sp_all_off()
{
    xdata uchar t[6], i, j=0;
    xdata uint  s;
    xdata ulong set=1000000;

    if (fngz || funder || (weight0 < szero) || hibyte(brut)) return;

    for (i=0; i<spmax; i++) t[i] = 0;
    for (i=0; i<spmax; i++){
        if (spdata[i] && spdata[i] > brut){
            if(spdata[i] < set) set = spdata[i];
        }else j++;
    }
    for (i=0; i<spmax; i++) if(spdata[i] && spdata[i] == set) t[i]=1; // more then one rel ave the same value
    sprel = 0;
    for (i=0; i<spmax; i++){
        if (t[i]){
            s = 1; s <<= i;
            sprel |= s;
        }
    }
    if (j == spmax){
        if (!frel8off){
            frel8off = 1;
            trel8off = dtime8;
        }
    }else frel8off = 0;
    if (spnew != sprel){
        tdelay = dtime;
        spnew = sprel;
    }
    if (fdelay1) fdelay1 = tdelay = 0; // first rel on start
    if (tdelay ) sprel   = 0;          // delay between rel
    if (!trel8off && frel8off){        // rel 8 on
        if (!frelsub8){
            if (osubrel8 &&  tstab) return;
            if (osubrel8 && !tstab) Total();
        }
        flock  = 1;
        fstart = 0;
        sprel  = 0x0080;               // rel 8 on
    }
}

void send_rel(uint imrel)
{
    if (!bytest){         // relay status tramsmission
        secbuf[0] = 0xfc ;
        secbuf[1] = byte(imrel, 1) ;
        secbuf[2] = checksum(secbuf[0], secbuf[1]) ;
        secbuf[3] = 0xfd ;
        secbuf[4] = byte(imrel, 0) ;
        secbuf[5] = checksum(secbuf[3], secbuf[4]) ;
        binstring(secbuf, 6);
    }
    getkey();
}

uchar   checksum(uchar ret1, uchar ret2)
{
    return(~(ret1+ret2) + 1) ;
}

void    getkey()
{
    xdata uchar   ret, res ;

    tneway = 1;
    bytest = 0;
    while(tneway){
        res = 0 ;
        if (comstat()){
            ret = getcom();
            res = 1;
        }
        if (res){
            switch(bytest){
                case 0: if (((ret & 0xfc) == 0xfc)&&((ret & 0x03) == 0x00)) bytest = 1 ; // from relsys 0
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

void    define_sp(void)
{
    xdata uchar  t;
    xdata ulong  x;
    
    if (pass){
        x = getdata("\n PASS ",0, 0, 0);
        if (x != pass){
            puts("\nERROR ");
            pause(10);
            return;
        }
    }

    clrscr() ;
    dmr = dml = 1;
    for (t = 0 ; t < spmax ; t++){
        sprintf(secbuf, "SET-%02d", (uint)t+1) ;
        spdata[t] = getdata(secbuf, spdata[t], disform, 0);
        if (cdouble('T')) break;
    }
    dmr = dml = 0;
    spdata0 = spdata[0];
    spdata1 = spdata[1];
    spdata2 = spdata[2];
    spdata3 = spdata[3];
    spdata4 = spdata[4];
    spdata5 = spdata[5];
    novsave(); pdone() ;
}

void    manual_sp(void)
{
    xdata uint    rel=0, ret=1;

    printf("\nREL 01") ;
    while(true){
        watchdog();
        if (kbhit()){
            switch(getch(0)){
                case 'T' :  rel ^= 1 << (ret-1) ;   break ;
                case 'Z' :  timeout = 0;
                            if (cdouble('Z')) ret = 0;
                            do{
                                if (!timeout){
                                    ret++;
                                    if (ret == (spmax+1)) ret = 7;
                                    if (ret > 8) ret = 1;
                                    timeout = 7;
                                }
                                if ((ret) && (rel&(1<<(ret-1)))) sprintf(ascbuf,"\nREL-%02d", ret);
                                else                             sprintf(ascbuf,"\nREL %02d", ret);
                                puts(ascbuf);
                                secbuf[0] = 0xfc; secbuf[1] = byte(rel, 1);
                                secbuf[2] = checksum(secbuf[0], secbuf[1]);
                                secbuf[3] = 0xfd; secbuf[4] = byte(rel, 0);
                                secbuf[5] = checksum(secbuf[3], secbuf[4]);
                                binstring(secbuf, 6);
                                watchdog();
                            }while(kzero); break ;
                case 'P' :  ret = rel = 0;
            }
            if ((ret) && (rel&(1<<(ret-1)))) sprintf(ascbuf,"\nREL-%02d", ret);
            else                             sprintf(ascbuf,"\nREL %02d", ret);
            puts(ascbuf);
        }
        secbuf[0] = 0xfc; secbuf[1] = byte(rel, 1);
        secbuf[2] = checksum(secbuf[0], secbuf[1]);
        secbuf[3] = 0xfd; secbuf[4] = byte(rel, 0);
        secbuf[5] = checksum(secbuf[3], secbuf[4]);
        binstring(secbuf, 6);
        getkey();
        if (!ret) return;
    }
}

void    rel_function(void)
{
    xdata uchar f=0;
    xdata ulong x;

    x = getdata(" 2ERO ", zerodiv, disform, 32000);
    if (zerodiv != x) f=1;
    zerodiv = x;
    if (!cdouble('T')){
        x = getdata("8 ON  ", dtime8, 133, 255);
        if (dtime8 != x) f=1;
        dtime8 = x;
        if (!cdouble('T')){
            x = getdata("DELAY ", dtime, 133, 250);
            if (dtime != x) f=1;
            dtime = x;
            if (!cdouble('T')){
                x = getdata("S.P - M", spmax, 0, 6) ;
                if (spmax != x) f=1;
                spmax = x;

                if (!cdouble('T')){
                    x = getdata(" PASS ", pass, 0, 9999) ;
                    if (pass != x) f=1;
                    pass = x;
                }

            }
        }
    }
    if (f) novsave();
    pdone() ;
}
