/**
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */

#include "watch.h"

void ResetTime( Watch* w ) 
{
    w->elapsed_time=0;
}

void TickTime( Watch* w, int time ) 
{
    w->elapsed_time+=time;
}

