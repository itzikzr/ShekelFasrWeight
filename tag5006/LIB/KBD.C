
#include "tigerl.inc"
#include "tigerl.h"

uchar   getch()
{
  while (!kbhit()) watchdog();
  fkey = 0;
  return (keylist[scancode]);
}

void    ungetch()
{
  scode = 0;
  fkey = 1;
}

uchar   keypress()
{
  uchar k;
  k = skey & 0X3F;
  if (k==0) {
     k = pkey;
     if (k) k |= 0X40;
  }
  if (k) k = keylist[k];
  return (k);
}

void    switchoff()     // power off
{
  clrscr();             // screen off
  discom = 9;           // back light off, and audible beep
  if (ocheck) disled = 0;
  else disled = -1;
  do {
     if (!INT1) timeout = 2;    // wait for key release
     watchdog();
  } while (timeout);
  INT1 = 0;
  while (!kbhit()) watchdog();
  while (1);
}