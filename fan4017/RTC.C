#include "fanbase.h"

bit     clockin(uchar *s, uchar n)
{
    icset(0);               // requesting the I2C bus
    n &= 3;
    n <<= 3;
    if (icout(rtcad))
    {
        if (icout(n))
        {
            SCL = 1;
            n = 0;
            if (icout(rtcad+1))
            {
                while (n<7) s[n++] = icin(1);
                s[n] = icin(0);
            }
        }
        else n = 0;
    }
    else n = 0;
    icstop();
    fsync = 1;              // releasing the I2C bus
    return (n);
}

bit     clockout(uchar n, uchar *s)
{
    icset(0);               // requesting the I2C bus
    n &= 3;
    n <<= 3;
    if (icout(rtcad))
    {
        if (icout(n))
        {
            n = 0;
            while (n<8)
            {
                if (icout(s[n])) n++;
                else break;
            }
            n &= 8;
        }
        else n = 0;
    }
    else n = 0;
    icstop();
    fsync = 1;              // releasing the I2C bus
    return (n);
}

// ÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜ
/*
    F1 - set the date & time values


     Description
   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
   The MK41T56 TimeKeeper RAM is a low power 512-bit static CMOS RAM,
   organized as 64 words by 8 bits. The first 8 bytes of the RAM are
   used for the clock / calendar function  and are configured in BCD
   format.
   Address map of RAM :   0.  Seconds Register  ( 0 - 59 )
                          1.  Minutes Register  ( 0 - 59 )
                          2.  Hours   Register  ( 0 - 23 )
                          3.  Dayweek Register  ( 1 - 7  )
                          4.  Day     Register  ( 1 - 31 )
                          5.  Month   Register  ( 1 - 12 )
                          6.  Years   Register  ( 0 - 99 )
                          7.  Control Register
                      8.-63.  Free RAM

*/

#define _second s[0]
#define _minute s[1]
#define _hour   s[2]
#define _numday s[3]   // week's day 
#define _day    s[4]
#define _month  s[5]
#define _year   s[6]
xdata  uchar    s[8];
xdata  uchar    buf[9];

uchar   day_of_week(struct date *sdate) ;
uchar   dec2bcd(uchar);
uchar   bcd2dec(uchar);
uchar   unput(uchar c);

void    set_time_end_date()
{
    strcpy(s,"       ");
    if (clockin (s  , 0)) datime();
}

uchar   settime(void)
{
    xdata uchar    t = 0 ;

    _second = dec2bcd(stime.ti_sec ) ;
    _minute = dec2bcd(stime.ti_min ) ;
    _hour   = dec2bcd(stime.ti_hour) ;
    _numday = day_of_week(&sdate) ;
    _day    = dec2bcd(sdate.da_day ) ;
    _month  = dec2bcd(sdate.da_mon ) ;
    _year   = dec2bcd(sdate.da_year%100) ;

    return(clockout(0, s)) ;
}

uchar   dec2bcd(uchar d)
{
    xdata uchar s[2], ret ;

    sprintf(s, "%02d", (uint)d) ;
    ret  =  s[1] - '0' ;
    ret += (s[0] - '0') << 4 ;
    return  ret  ;
}
uchar   day_of_week(struct date *sdate)
{
//  01-01-1990 - Monday (1)

    xdata uint    leap   ;
    xdata uint    d, m[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } ;
    xdata long    t = 0  ;

    for (d = 1990 ; d < sdate->da_year ; d ++ )
    {
        leap = (d % 4 == 0) && (d % 100 != 0) || (d % 400 == 0) ;
        if (leap) t += 366 ;
        else  t += 365 ;
    }
    leap = (d % 4 == 0) && (d % 100 != 0) || (d % 400 == 0) ;
    if (leap) m[1] = 29 ; else m[1] = 28 ;
    for (d = 01 ; d < sdate->da_mon ; d ++ )
    {
        t += m[d-1] ;
    }
    t += sdate->da_day ;
    d  =  t % 7 ;
    if (!d) d = 7 ;
    return( d ) ;
}

uchar   gettime(void)
{
    xdata uchar    t = 7 ;
    xdata uchar   *pt = s ;

    if (!clockin(s, 0))  return(0) ;
    stime.ti_sec   =  bcd2dec(_second) ;
    stime.ti_min   =  bcd2dec(_minute) ;
    stime.ti_hour  =  bcd2dec(_hour  ) ;
    sdate.da_week  =  bcd2dec(_numday) ;
    sdate.da_day   =  bcd2dec(_day   ) ;
    sdate.da_mon   =  bcd2dec(_month ) ;
    sdate.da_year  = (bcd2dec(_year  ) < 95) ? bcd2dec(_year) + 2000 :
    bcd2dec(_year) + 1900 ;
    return(1) ;
}
uchar   bcd2dec(uchar bcd)
{
    return (bcd >> 4) * 10 + (bcd&0x0f) ;
}

void    get_date_time(uchar *, uchar m[9]);

void    datime(void)
{
    xdata uchar msg[9], ch,f=0;
    xdata uchar date[9], time[9];

    if (gettime() && controlDate())
    {
        sprintf(date, "%02d.%02d.%02d", (uint)sdate.da_day,
                (uint)sdate.da_mon,
                (uint)sdate.da_year%100) ;
        sprintf(time, "%02d.%02d.%02d", (uint)stime.ti_hour,
                (uint)stime.ti_min ,
                (uint)stime.ti_sec);
    }
    else
    {
        sprintf(date, "11.11.11") ;
        sprintf(time, "00.00.00") ;
    }
    sprintf(msg,"\n DATE ");
    blink(msg, date, 10);
    while(true){
        watchdog();
        ch = kbhit() ? getch(0) : 0 ;
        switch(ch){
            case 'P': return;
            case 'Z': f=1;
                      get_date_time(msg, date);  
                      break;

            case 'T': if (f) sscanf_date(buf);
                      else   sscanf_date(date);
                      sdate.da_year += (sdate.da_year < 95) ? 2000 : 1900 ;
                      if (!controlDate()){
                          printf("\nNO.DATE");
                          break ; 
                      }else settime() ;

                      sprintf(msg, "\n TIME ");
                      blink(msg, time, 10);
                      get_date_time(msg, time);
                      sscanf_time(buf);
                      if (!controlTime()) break ; 
                      if (settime()) pdone(); 
                      else  puts("\nERROR ") ;
                      return ;    
        }
    }
}

void    sscanf_date(uchar *date)
{
    xdata uchar  datetime[9] ;

    strcpy(datetime, date) ;
    datetime[2]  = datetime[5] = 0 ;
    sdate.da_day  = (uchar) atoi(datetime + 0) ;
    sdate.da_mon  = (uchar) atoi(datetime + 3) ;
    sdate.da_year = (uint ) atoi(datetime + 6) ;
}

uint    controlDate(void)
{
    xdata uint    leap   ;
    xdata uint    d, m[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } ;

    d = sdate.da_year ;
    leap = (d % 4 == 0) && (d % 100 != 0) || (d % 400 == 0) ;
    if (leap) m[1] = 29 ; else m[1] = 28 ;
    if (sdate.da_mon == 0)   return(false) ;
    if (sdate.da_mon > 12)   return(false) ;
    d = sdate.da_mon - 1 ;
    if (sdate.da_day == 0)   return(false) ;
    if (sdate.da_day > m[d]) return(false) ;
    return(true) ;
}

void    sscanf_time(uchar *time)
{
    xdata uchar  datetime[9] ;

    strcpy(datetime, time) ;
    datetime[2]  = datetime[5] = 0 ;
    stime.ti_hour = (uchar) atoi(datetime + 0) ;
    stime.ti_min  = (uchar) atoi(datetime + 3) ;
    stime.ti_sec  = (uchar) atoi(datetime + 6) ;
    stime.ti_hund =  0 ;
}

uint    controlTime(void)
{
    if (stime.ti_hour > 23)  return(false) ;
    if (stime.ti_min  > 59)  return(false) ;
    if (stime.ti_sec  > 59)  return(false) ;
    return(true) ;
}

void get_date_time(uchar *s, uchar m1[9])
{
    xdata uchar d, fdate,f;
    xdata ulong m, n;
    xdata uchar msg[20];

    m = 999999L;
    talter = falter = timout = 0;
    d = n = f = 0;
    if (!strcmp("\n DATE ",s)) fdate = 1;
    else                       fdate = 0;
    do{
        if (testbit(fkey)){
            talter = hafsec;
            if (timout){
                if (kzero){
                    if (!f) m1[0] = 0;        
                    f = 1;
                    n += d;  n *= 10;  d = 0;
                    if (n > m) n = 0;
                    sprintf(msg,"%06ld",n);
                    if (fdate){
                        m1[0] = msg[0]; m1[1] = msg[1]; m1[2] = '.';
                        m1[3] = msg[2]; m1[4] = msg[3]; m1[5] = '.';
                        m1[6] = msg[4]; m1[7] = msg[5]; m1[8] = 0;
                    }else{
                        m1[0] = msg[2]; m1[1] = msg[3]; m1[2] = '.';
                        m1[3] = msg[4]; m1[4] = msg[5]; m1[5] = '.';
                        m1[6] = '0'; m1[7] = '0'; m1[8] = 0;
                    }
                    printf("\n%s", m1);
                }
                timout = 0;
            }
            else timout = hafsec;
        }
        if (kzero){
            if (!talter){
                if (!f) m1[0] = 0;
                f = 1;
                talter = hafsec;
                if ((++d) > 9) d = 0;
                if ((n+d) > m) d = 0;
                sprintf(msg,"%06ld",n+d);
                if (fdate){
                    m1[0] = msg[0]; m1[1] = msg[1]; m1[2] = '.';
                    m1[3] = msg[2]; m1[4] = msg[3]; m1[5] = '.';
                    m1[6] = msg[4]; m1[7] = msg[5]; m1[8] = 0;
                }else{
                    m1[0] = msg[2]; m1[1] = msg[3]; m1[2] = '.';
                    m1[3] = msg[4]; m1[4] = msg[5]; m1[5] = '.';
                    m1[6] = '0'; m1[7] = '0'; m1[8] = 0;
                }
                printf("\n%s", m1);
            }
        }
        else talter = hafsec;
        watchdog();
    }
    while (!ktare && !kprint);
    strcpy(buf,m1);
}
