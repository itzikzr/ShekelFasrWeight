
#include "tigerl.inc"
#include "tigerl.h"

/*
            ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
            ³                                 ³Û
            ³    A/D and TRIMMER functions    ³Û
            ³                                 ³Û
            ÀÄÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÛ
*/

void    wait(uchar n)
{
  while (n--);                  // total wait time = 5 * (n+2) usec
}

bit     calib()
{
  acal = 0; nop; acal = 1; wait(50); timeout = 20;
  while ((adata==0)&&(timeout)) watchdog();
  fsamp = 0;
  return (adata);
}

bit    trimset(uchar n) small
{
  uchar i;
  asel = 1;
  for (i=0;i<100;i++) { acal = 0; wait(2); acal = 1; }
  acal = 0; scl = 0;
  while (n--) { acal = 1; wait(1); acal = 0; }
  scl = 1; asel = 0;
  return (calib());
}
#define hafstep 305
uchar  	trimfix(uint target)
{
  extern xdata uint atod ;
  uchar hitrim, lotrim, gotrim;

  lotrim = 0;
  hitrim = 100;
  do {
     gotrim = hitrim + lotrim + 1;
     gotrim >>= 1;
     printf("\n =%2u= ", (uint) gotrim);
     if (!trimset(gotrim)) {
        hitrim = lotrim = gotrim = 0;
     }
     atod = atodin() ;
     if (atod<target) hitrim = gotrim;
     else lotrim = gotrim;
  } while ((hitrim-lotrim)>1);
  if (atod > target+hafstep) {
     printf("\n =%2u= ", (uint) ++gotrim);
  }  else
  if (atod < target-hafstep) {
     printf("\n =%2u= ", (uint) --gotrim);
  }
  if (!trimset(gotrim)) gotrim = 0;
  return (gotrim);
}
