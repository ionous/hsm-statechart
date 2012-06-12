/**
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __WATCH_H__
#define __WATCH_H__

typedef struct watch_object Watch;
struct watch_object
{
    int elapsed_time;
};

void ResetTime( Watch* );
void TickTime( Watch*, int time );


#endif // #ifndef __WATCH_H__
