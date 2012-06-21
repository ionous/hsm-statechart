/**
 * @file watch1_named_events.c
 * Copyright everMany, LLC. 2012
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "hsm_machine.h"    // the state machine
#include "watch.h"          // watch object
#include "platform.h"       // console input/output

#include <stdlib.h>
#include <stdio.h>

//---------------------------------------------------------------------------
// a simple example of shared strings just for this sample
// in pure c, if these were included in multiple files, they'd all get their own copies
static const char * WATCH_TICK= "Tick";
static const char * WATCH_RESET_PRESSED= "Reset";
static const char * WATCH_TOGGLE_PRESSED= "Toggle";

typedef struct hsm_event_rec WatchEvent;
struct hsm_event_rec {
    const char * name;
};

typedef struct tick_event TickEvent;
struct tick_event {
    WatchEvent evt;
    int time;
};
typedef struct watch_context WatchContext;
struct watch_context {
    hsm_context_t ctx;
    Watch * watch;
};

//---------------------------------------------------------------------------
HSM_STATE_ENTER( Active, HsmTopState, Stopped  );
    HSM_STATE( Stopped, Active, 0 ); 
    HSM_STATE( Running, Active, 0 );
    
//---------------------------------------------------------------------------
hsm_context ActiveEnter( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    Watch* watch=((WatchContext*)ctx)->watch;
    ResetTime( watch );
    return ctx;
}

//---------------------------------------------------------------------------
hsm_state ActiveEvent( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    hsm_state ret=NULL;
    // on reset self-transition
    if (evt->name == WATCH_RESET_PRESSED) {
        ret= Active();
    }
    return ret;
}

//---------------------------------------------------------------------------
hsm_state StoppedEvent( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    hsm_state ret=NULL;
    if (evt->name == WATCH_TOGGLE_PRESSED) {
        ret= Running();
    }            
    return ret;
}

//---------------------------------------------------------------------------
hsm_state RunningEvent( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    hsm_state ret=NULL;
    if (evt->name == WATCH_TOGGLE_PRESSED) {
        ret= Stopped();
    }
    else
    if (evt->name == WATCH_TICK) {
        Watch* watch=((WatchContext*)ctx)->watch;
        TickEvent* tick= (TickEvent*)evt;
        TickTime ( watch, tick->time );
        printf("%d,", watch->elapsed_time );
        ret= HsmStateHandled();
    }
    return ret;
}

//---------------------------------------------------------------------------
/**
 * 
 */
int watch1_named_events( int argc, char* argv[] )
{   
    hsm_context_machine_t machine;
    Watch watch;
    WatchContext ctx= { 0, &watch };
    hsm_machine hsm= HsmMachineWithContext( &machine, &ctx.ctx );
    
    printf( "Stop Watch Sample.\n"
        "Keys:\n"
            "\t'1': reset button\n"
            "\t'2': generic toggle button\n" );
    
    HsmStart( hsm, Active() );

    while ( HsmIsRunning( hsm ) ) {
        int ch= PlatformGetKey();
        if (ch) {
            if (ch=='1') {
                WatchEvent evt= { WATCH_RESET_PRESSED };
                HsmProcessEvent( hsm, &evt );
                printf(".");
            }
            else 
            if (ch=='2') {
                WatchEvent evt= { WATCH_TOGGLE_PRESSED };
                HsmProcessEvent( hsm, &evt );
                printf(".");
            }
        }
        else {
            TickEvent tick= { WATCH_TICK, 1 };
            HsmProcessEvent( hsm, &tick.evt );
            PlatformSleep(500);
        }
    };
    return 0;
}
