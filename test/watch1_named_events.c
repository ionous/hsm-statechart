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
#include "hsm_machine.h"    // for the state machine
#include "hsm_context.h"    // sample uses context data
#include "hsm_state.h"       // for state declarations
#include "watch.h"
#include "platform.h"

#include <stdlib.h>
#include <stdio.h>

//---------------------------------------------------------------------------
// a simple example of shared strings just for this sample
// in pure c, if these were included in multiple files, they'd all get their own copies
static const char * WATCH_TICK= "Tick";
static const char * WATCH_RESET_PRESSED= "Reset";
static const char * WATCH_TOGGLE_PRESSED= "Toggle";

typedef struct hsm_event WatchEvent;
struct hsm_event {
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
HSM_STATE_ENTER( ActiveState, HsmTopState, StoppedState  );
    HSM_STATE( StoppedState, ActiveState, 0 ); 
    HSM_STATE( RunningState, ActiveState, 0 );
    
//---------------------------------------------------------------------------
hsm_context_t* ActiveStateEnter( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    Watch* watch=((WatchContext*)ctx)->watch;
    ResetTime( watch );
    return ctx;
}

//---------------------------------------------------------------------------
hsm_state ActiveStateEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    hsm_state ret=NULL;
    // on reset self-transition
    if (evt->name == WATCH_RESET_PRESSED) {
        ret= ActiveState();
    }
    return ret;
}

//---------------------------------------------------------------------------
hsm_state StoppedStateEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    hsm_state ret=NULL;
    if (evt->name == WATCH_TOGGLE_PRESSED) {
        ret= RunningState();
    }            
    return ret;
}

//---------------------------------------------------------------------------
hsm_state RunningStateEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    hsm_state ret=NULL;
    if (evt->name == WATCH_TOGGLE_PRESSED) {
        ret= StoppedState();
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
    hsm_machine_t hsm;
    hsm_context_stack_t stack;
    Watch watch;
    WatchContext ctx= { 0, &watch };
    
    printf( "Stop Watch Sample.\n"
        "Keys:\n"
            "\t'1': reset button\n"
            "\t'2': generic toggle button\n" );
    
    HsmMachine( &hsm, &stack, NULL );
    HsmStart( &hsm, &ctx.ctx, ActiveState() );

    while ( HsmIsRunning( &hsm ) ) {
        int ch= PlatformGetKey();
        if (ch) {
            if (ch=='1') {
                WatchEvent evt= { WATCH_RESET_PRESSED };
                HsmProcessEvent( &hsm, &evt );
                printf(".");
            }
            else 
            if (ch=='2') {
                WatchEvent evt= { WATCH_TOGGLE_PRESSED };
                HsmProcessEvent( &hsm, &evt );
                printf(".");
            }
        }
        else {
            TickEvent tick= { WATCH_TICK, 1 };
            HsmProcessEvent( &hsm, &tick.evt );
            PlatformSleep(500);
        }
    };
    return 0;
}
