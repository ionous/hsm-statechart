/**
 * @file watch_builder.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "hsm_machine.h"    // the state machine
#include "hsm_builder.h"
#include "watch.h"          // watch object
#include "platform.h"       // console input/output

#include <stdlib.h>
#include <stdio.h>


// declare an enum of all possible watch events.
// this is just one way to declare events.
// see also watch1_named_events.c
typedef enum watch_events WatchEvents;
enum watch_events {
    WATCH_RESET_PRESSED,
    WATCH_TOGGLE_PRESSED,
    WATCH_TICK,
};

// it's up to the user code to define the hsm_event structure.
// again, this is just one way to declare events,
// you could also wrap the events of another framework if you wanted.
typedef struct hsm_event_rec WatchEvent;
struct hsm_event_rec {
    WatchEvents evt;
};

// the tick event extends the basic watch event to add elapsed time.
// you can have as many different kinds of event data as you want.
typedef struct tick_event TickEvent;
struct tick_event {
    WatchEvent evt;
    int time;
};

// a context object gets sent to every callback of the statemachine
// you always need to have an hsm_context_t member as the first element
// after that, it's up to you what you want to include.
// in this case: a pointer to the watch object.
typedef struct watch_context WatchContext;
struct watch_context {
    hsm_context_t ctx;
    Watch * watch;
};

//---------------------------------------------------------------------------
static hsm_context ActiveStateEnter( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    Watch* watch=((WatchContext*)ctx)->watch;
    ResetTime( watch );
    return ctx;
}

//---------------------------------------------------------------------------
static void RunTickTime( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    // our context is the watch object
    Watch* watch= ((WatchContext*)ctx)->watch;
    // our event is the tick event
    TickEvent * tick= ((TickEvent*)evt);
    // tick by this much time
    TickTime ( watch, tick->time );
    // print the total time
    printf("%d,", watch->elapsed_time );
}

//---------------------------------------------------------------------------
int buildWatchMachine(int parent) 
{
    const int active= hsmState( "Active" );
    hsmBegin( active );
    {
        const int stopped= hsmState( "Stopped" ),
                  running= hsmState( "Running" );

        hsmOnEnter( 
            ActiveStateEnter );

        hsmOni( WATCH_RESET_PRESSED );
        hsmGoto( running );

        hsmBegin( stopped );
        {
            hsmOni( WATCH_TOGGLE_PRESSED );
            hsmGoto( running );
        }
        hsmEnd();

        hsmBegin( running );
        {
            hsmOni( WATCH_TOGGLE_PRESSED );
            hsmGoto( stopped );

            hsmOni( WATCH_TICK );
            hsmRun( RunTickTime );
        }
        hsmEnd();
    }
    hsmEnd();
    return active;
}
