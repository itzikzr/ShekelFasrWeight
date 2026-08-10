#include "fanbase.h"

void sampl_rate();
void filter();
void noise();
void filter2();
void zeromode();
void zero_target();
void uper_zero_on_startup();
void lower_zero_on_startup();
void uper_zero_normal_startup();
void lower_zero_normal_startup();
void zero_track_time();
void zero_track_range();
void satble_time();
void profident();
void HidOp();

void hidemode()
{
    dpcs = dkg = dlb = dbat = dzero = dtare = fsave = 0;
    if (comstat()) getcom();
    if (fun==NULL){
        fun = sampl_rate;
        num = filter0 & 0xc0;
        num = num >> 6;
    }
    fun();
}

void sampl_rate()
{
    if (num > 3) num = 0;
    switch(num){
        case 0: printf("\nSAMP 0"); break;
        case 1: printf("\nSAMP 1"); break;
        case 2: printf("\nSAMP 2"); break;
        case 3: printf("\nSAMP 3"); break;
    }
    if (kbhit()){
        switch (getch(0))
        {
            case 'Z': num++; break;
            case 'T': fun = filter; 
                      num = num << 6;
                      if (num != (filter0 & 0xc0)){
                          fprog = 1;
                          filter0 &= 0x3f;
                          filter0 |= num;
                      }
                      num = filter0 & 0x30;
                      num = num >> 4;
                      break;
            case 'P': reboot();
        }
    }
}

void filter()
{
    if (num > 3) num = 0;
    switch(num){
        case 0: printf("\nFIL1 0"); break;
        case 1: printf("\nFIL1 1"); break;
        case 2: printf("\nFIL1 2"); break;
        case 3: printf("\nFIL1 3"); break;
    }   
    if (kbhit()){
        switch (getch(0))
        {
            case 'Z': num++; break;
            case 'T': fun = noise; 
                      num = num << 4;
                      if (num != (filter0 & 0x30)){
                          fprog = 1;
                          filter0 &= 0xcf;
                          filter0 |= num; 
                      }
                      num = filter0 & 0x08;
                      num = num >> 3;
                      break;
            case 'P': reboot();
        }
    }
}

void noise()
{
    if (num > 1) num = 0;
    switch(num){
        case 0: printf("\nNOIS 0"); break;
        case 1: printf("\nNOIS 1"); break;
    }   
    if (kbhit()){
        switch (getch(0))
        {
            case 'Z': num++; break;
            case 'T': fun = filter2; 
                      num = num << 3;
                      if (num != (filter0 & 0x08)){
                          fprog = 1;
                          filter0 &= 0xf7;
                          filter0 |= num; 
                      }
                      num = filter0 & 0x07;
                      break;
            case 'P': reboot();
        }
    }
}

void filter2()
{
    if (num > 7) num = 0;
    switch(num){
        case 0: printf("\nFIL2 0"); break;
        case 1: printf("\nFIL2 1"); break;
        case 2: printf("\nFIL2 2"); break;
        case 3: printf("\nFIL2 3"); break;
        case 4: printf("\nFIL2 4"); break;
        case 5: printf("\nFIL2 5"); break;
        case 6: printf("\nFIL2 6"); break;
        case 7: printf("\nFIL2 7"); break;
    }   
    if (kbhit()){
        switch (getch(0))
        {
            case 'Z': num++; break;
            case 'T': fun = zeromode; 
                      if (num != (filter0 & 0x07)){
                          fprog = 1;
                          filter0 &= 0xf8;
                          filter0 |= num; 
                      }
                      num = zmode0;
                      break;
            case 'P': reboot();
        }
    }
}

void zeromode()
{
    if (num != 1 && num != 0) num = 0;
    switch(num){
        case 0: printf("\nZ.CAL 0"); break;
        case 1: printf("\nZ.CAL 1"); break;
    }   
    if (kbhit()){
        switch (getch(0))
        {
            case 'Z': num ^= 1; break;
            case 'T': fun = zero_target;
                      if (num != zmode0){
                          fprog  = 1;
                          zmode0 = num;
                      }
                      break;
            case 'P': reboot();
        }
    }
}

void zero_target()
{
    progvar = target0;
    hibyte(progvar) = 0;
    progvar = getdata("\nZ.TARGT", progvar, 0, 0);
    hibyte(progvar) = zmode0;
    if (progvar != target0) fprog = 1;
    target0 = progvar;
   
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = uper_zero_on_startup;
    }
    if (kprint) reboot();
}

void uper_zero_on_startup()
{
    progvar = hiabs0;
    hibyte(progvar) = 0;
    progvar = getdata("\nZ UP.ON", progvar, 0, 99);
    if (progvar != hiabs0) fprog = 1;
    hiabs0 = progvar;
   
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = lower_zero_on_startup;
    }
}

void lower_zero_on_startup()
{
    progvar = loabs0;
    hibyte(progvar) = 0;
    progvar = getdata("\nZ LO.ON", progvar, 0, 99);
    if (progvar != loabs0) fprog = 1;
    loabs0 = progvar;
   
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = uper_zero_normal_startup;
    }
}

void uper_zero_normal_startup()
{
    progvar = hirel0;
    hibyte(progvar) = 0;
    progvar = getdata("\nZ UPER", progvar, 0, 99);
    if (progvar != hirel0) fprog = 1;
    hirel0 = progvar;
   
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = lower_zero_normal_startup;
    }
}

void lower_zero_normal_startup()
{
    progvar = lorel0;
    hibyte(progvar) = 0;
    progvar = getdata("\nZ LOU ", progvar, 0, 99);
    if (progvar != lorel0) fprog = 1;
    lorel0 = progvar;
   
    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = satble_time;
    }
}

void satble_time()
{
    progvar = (ulong)stable0;
    hibyte(progvar) = 0;
    progvar = getdata("\nSTABEL", progvar, 0, 99);
    if (progvar != stable0) fprog = 1;
    stable0 = progvar;

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = zero_track_range;
    }
    if (kprint) reboot();
}

void zero_track_range()
{
    progvar = (ulong)zrange0;
    hibyte(progvar) = 0;
    progvar = getdata("\nZT.RANG", progvar, 0, 99);
    if (progvar != zrange0) fprog = 1;
    zrange0 = progvar;

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = zero_track_time;
    }
    if (kprint) reboot();
}

void zero_track_time()
{
    progvar = (ulong)ztime0;
    hibyte(progvar) = 0;
    progvar = getdata("\nZT.TIME", progvar, 0, 99);
    if (progvar != ztime0) fprog = 1;
    ztime0 = progvar;

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else{
            if (oprofi && !onet) fun = profident;
            else                 fun = HidOp;
        }
    }
    if (kprint) reboot();
}

void profident()
{
    progvar = (ulong)profiID;
    hibyte(progvar) = 0;
    progvar = getdata("\nPRF ID", progvar, 0, 0);
    if (progvar != profiID) fprog = 1;
    profiID = progvar;

    if (ktare){
        if (cdouble('T')){
            if (fprog) gosave();
            else       reboot();
        }
        else fun = HidOp;
    }
    if (kprint) reboot();
}

bit ChOpOn(uchar i);
void HidOp()
{
    xdata uchar i=0, j=0, op1, l, n;

    op1 = op0;
    j = ChOpOn(i);
    n = 3; // op num
    printf("\nHOP.%1u-%1u",(uint)i+1,(uint)j);
    do{
        if (kbhit()){
            switch(getch(0)){
                case 'Z': if(cdouble('Z'))
                          {
                              j = j ^ 1;
                              l = 1;                              
                              l <<= i; 
                              op0  = op0 ^ l;
                              printf("\nHOP.%1u-%1u",(uint)i+1,(uint)j);
                          }
                          else{
                              timeout = 0;
                              do{
                                  watchdog();
                                  if (!timeout){
                                      timeout = 3;
                                      if (i >= n-1) i = 0; 
                                      else          i++;
                                      j = ChOpOn(i);
                                      printf("\nHOP.%1u-%1u",(uint)i+1,(uint)j);
                                  }
                              }
                              while(kzero);
                          }
                          break ;
                case 'T': if (op0 != op1) fprog = 1; 
                          if (fprog) gosave();
                          else       reboot();
                          return;
                }
        }
        watchdog();
    }
    while(1);
}

bit ChOpOn(uchar i)
{
    xdata ulong x, y=1;

    x = op0;
    y <<= i;
    if (x&y) return 1;
    else     return 0;
}