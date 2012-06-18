/**
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "platform.h"
#include <stdio.h>
#include <unistd.h>

//http://cboard.cprogramming.com/c-programming/63166-kbhit-linux.html
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;
 
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON| ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
 
  ch = getchar();
 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);
 
  if(ch != EOF)
  {
    //ungetc(ch, stdin);
    return ch;
  }
 
  return 0;
}

//---------------------------------------------------------------------------
int PlatformGetKey()
{
    return kbhit();//? getchar() : 0;
}

//---------------------------------------------------------------------------
void PlatformSleep(int milli )
{
    usleep( milli * 1000 );
}

//---------------------------------------------------------------------------
void PlatformBeep()
{
    putchar('\a') ;
}
