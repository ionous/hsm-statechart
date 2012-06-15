/**
 * @file watch1_named_events.c
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

typedef enum watch_events WatchEvents;
enum watch_events {
    WATCH_RESET_PRESSED,
    WATCH_TOGGLE_PRESSED,
    WATCH_TICK,
};

typedef struct hsm_event WatchEvent;
struct hsm_event {
    WatchEvents evt;
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
// declare the states
HSM_STATE_ENTER( ActiveEnum, HsmTopState, StoppedEnum );
    HSM_STATE( StoppedEnum, ActiveEnum, 0 );
    HSM_STATE( RunningEnum, ActiveEnum, 0 );

//---------------------------------------------------------------------------
hsm_context_t* ActiveEnumEnter( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    Watch* watch=((WatchContext*)ctx)->watch;
    ResetTime( watch );
    return ctx;
}

//---------------------------------------------------------------------------
hsm_state ActiveEnumEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    hsm_state ret=NULL;
    switch ( evt->evt ) {
        case WATCH_RESET_PRESSED:
            ret= ActiveEnum();
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
hsm_state StoppedEnumEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    hsm_state ret=NULL;
    switch (evt->evt) {
        case WATCH_TOGGLE_PRESSED:
            ret= RunningEnum();
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
hsm_state RunningEnumEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    hsm_state ret=NULL;
    switch (evt->evt) {
        case WATCH_TOGGLE_PRESSED:
            ret= StoppedEnum();
        break;
        case WATCH_TICK:
        {
            Watch* watch=((WatchContext*)ctx)->watch;
            TickEvent* tick= (TickEvent*)evt;
            TickTime ( watch, tick->time );
            printf("%d,", watch->elapsed_time );
            ret= HsmStateHandled();
        }
        break;
    }
    return ret;
}
//---------------------------------------------------------------------------
/**
 * 
 */
int watch1_enum_events( int argc, char* argv[] )
{   
    hsm_machine_t hsm;
    hsm_context_stack_t stack;
    Watch watch;
    WatchContext ctx= { 0, &watch };
    
    printf( "Stop Watch Sample with Enum Events.\n"
        "Keys:\n"
            "\t'1': reset button\n"
            "\t'2': generic toggle button\n" );
    
    HsmMachine( &hsm, &stack, NULL );
    HsmStart( &hsm, &ctx.ctx, ActiveEnum() );

    while ( HsmIsRunning( &hsm ) ) {
        int ch= PlatformGetKey();
        WatchEvents events[]= { WATCH_RESET_PRESSED, WATCH_TOGGLE_PRESSED };
        int index= ch-'1';
        if ((index >=0) && (index < sizeof(events)/sizeof(WatchEvents))) {
            WatchEvent evt= { events[index] };
            HsmProcessEvent( &hsm, &evt );
            printf(".");
        }
        else {
            TickEvent tick= { WATCH_TICK, 1 };
            HsmProcessEvent( &hsm, &tick.evt );
            PlatformSleep(500);
        }
    };
    return 0;
}