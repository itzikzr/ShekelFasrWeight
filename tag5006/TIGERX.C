#include "tiger.h"
/*
ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
³                 'File system' in memory                  ³Û
ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/
//  search the 'code' data in 'mcode' area
//  return 'bit' search result and address (0 - mptr = moef, 1 - actual mptr)
bit fexist(uchar *scode)
{
    uchar *ptr ;

    ptr = &mcode ;
    while(ptr < meof){
        watchdog() ;
        mptr = (struct mdat *)ptr ;
        if  (mptr->md_ferase == ' ')
            if  (!strcmp(mptr->md_code, scode))  return(1) ;
        ptr += dp + mptr->md_spmax * 4 ;
    }
    mptr = (struct mdat *)meof ;
    return(0) ;
}
//   erase 'code' data from 'mcode' area
bit ferase(uchar *scode)
{
    uchar  *ptr1, *ptr2 ;
    uint    dif ;

    if (fexist(scode)){
        ptr1 = (uchar *)mptr + mptr->md_spmax*4 + dp ;
        dif  = meof - ptr1 ;
        ptr2 = (uchar *)mptr ;
        memlove(ptr2, ptr1, dif) ;      // my own memmove - Leo
        meof = ptr2 + dif ;
        return(true) ;
    }
    else  return(false) ;
}
//   write 'code' data to 'mcode' area
#pragma OPTIMIZE (4)
bit fwrite(uchar *scode)
{
    uchar  *peof, s[25] ;
    uchar  *ptr1, *ptr2 ;
    int max, dif, t ;

    if (ocheck)
        if (target) max = 4 ;
            else    max = 0 ;
               else max = spmax ;

    sprintf(s, "%s - RAM FULL", scode) ;
    if (fexist(scode)){
        if (max != mptr->md_spmax){
            ptr1 = (uchar *)mptr ;
            ptr1 = ptr1 + mptr->md_spmax*4 + dp ;
            dif  = meof - ptr1 ;
            ptr2 = (uchar *)mptr ;
            ptr2 = ptr2 + max * 4 + dp ;
            peof = ptr2 + dif ;
            if (peof < (mcode + marea)){
                memlove(ptr2, ptr1, dif) ;
                meof = peof ;
            }
            else{
                message(s, 2) ; return(0) ;
            }
        }
    }
    else{
        peof = (uchar*)mptr + max * 4 + dp  ;
        if (peof < (mcode + marea)){
            mptr->md_ferase = ' ' ;
            strcpy(mptr->md_code, scode) ;
            meof += max * 4 + dp  ;
        }
        else{
            message(s, 2) ; return(0) ;
        }
    }
    mptr->md_unit    = kunit ;
    mptr->md_tare    = tare  ;
    mptr->md_order   = order ;
    mptr->md_total   = total ;
    mptr->md_gorder  = gorder;
    mptr->md_gtotal  = gtotal;
    mptr->md_ranlo   = ranlo ;
    mptr->md_ranhi   = ranhi ;
    mptr->md_spmax   = max ;
    mptr->md_zerodiv = zerodiv;
    mptr->md_zeroneg = zeroneg;
    mptr->md_wc      = fwc ;
    if (ocheck){
        if (target){
            mptr->md_sp[0] = target;
            mptr->md_sp[1] = tollo ;
            mptr->md_sp[2] = tolhi ;
            mptr->md_sp[3] = greendiv ;
        }
    }
    else{
        for (t = 0 ; t < spmax ; t++)  mptr->md_sp[t] = spdata[t] ;
    }
    return(1) ;
}
#pragma OPTIMIZE (5)
//   read existed 'code' from 'mcode' memory
bit  fread(uchar *scode)
{
    uint t ;
    bit  f ;

    if (f = fexist(scode)){
        kunit   = mptr->md_unit  ;
        tare    = mptr->md_tare  ;
        order   = mptr->md_order ;
        total   = mptr->md_total ;
        gorder  = mptr->md_gorder;
        gtotal  = mptr->md_gtotal;
        ranlo   = mptr->md_ranlo ;
        ranhi   = mptr->md_ranhi ;
        spmax   = mptr->md_spmax ;
        zerodiv = mptr->md_zerodiv;
        zeroneg = mptr->md_zeroneg;
        fwc  = mptr->md_wc ;
    }
    else{
        kunit = tare = order = 0 ;
        total = gorder = gtotal = 0 ;
        ranlo = ranhi = spmax = fwc = 0 ;
        zerodiv = zeroneg = 0;
    }
    t  = (spmax) ? 1 : 0 ; t <<= 1 ;
    t += (uint ) ocheck  ;
    switch(t){
        case 0 :                 // spmax = 0, setpoints
                for (t = 0 ; t < spnum ; t++) spdata[t] = 0 ;
                break  ;
        case 1 :                             // spmax = 0, checkweigher
                target = tollo = tolhi = greendiv = 0 ;
                break  ;
        case 2 :                             // spmax > 0, setpoints
                for (t = 0 ; t < spmax ; t++) spdata[t] = mptr->md_sp[t] ;
                for (t = t ; t < spnum ; t++) spdata[t] = 0 ;
                break ;
        case 3 :                             // spmax > 0, checkweigher
                target = mptr->md_sp[0] ;
                tollo  = mptr->md_sp[1] ;
                tolhi  = mptr->md_sp[2] ;
                greendiv = mptr->md_sp[3] ;
                
                spdata[0] = target  ;
                spdata[1] = tollo   ;
                spdata[2] = tolhi   ;
                spdata[3] = greendiv;
    }
    if (ocount){
        if (fwc==1) fcountstart = 1;
        fcount = !(kunit>0) ;
        toggle0() ;
        fcountstart = 0;
    }
    return(f) ;
}
//  number of codes
uint    fquantity(void)
{
    uchar  *ptr ;
    struct mdat *sptr ;
    uint    n = 0 ;

    ptr = &mcode ;
    while(ptr < meof){
        sptr = (struct mdat *)ptr ;
        if (sptr->md_ferase == ' ')  n++ ;
        ptr += dp + sptr->md_spmax * 4 ;
    }
    return(n) ;
}
