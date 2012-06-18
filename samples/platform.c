/**
 * @file platform.c
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "platform.h"
#include <windows.h>
#include <conio.h>

//---------------------------------------------------------------------------
int PlatformGetKey()
{
    return _kbhit()? _getch() : 0;
}

//---------------------------------------------------------------------------
void PlatformSleep(int milli )
{
    Sleep( milli );
}

//---------------------------------------------------------------------------
void PlatformBeep()
{
    _putch('\a') ;
}
