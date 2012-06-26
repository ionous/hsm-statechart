/**
 * @file watch_builder.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include <hsm/hsm_machine.h>    // the state machine
#include <hsm/builder/hsm_builder.h>
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
    WatchEvents type;
};

// the tick event extends the basic watch event to add elapsed time.
// you can have as many different kinds of event data as you want.
typedef struct tick_event TickEvent;
struct tick_event {
    WatchEvent core;
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
static hsm_context ActiveStateEnter( hsm_status status )
{
    Watch* watch=((WatchContext*)status->ctx)->watch;
    ResetTime( watch );
    return status->ctx;
}

//---------------------------------------------------------------------------
/**
 * NOTE: the second parameter is `user data`
 * but this user data is shared for *all* watches
 * b/c this chart can be in use simultaneously by multiple watches.
 *
 * its only status->hsm ( the machine ) and status->ctx ( the context data )
 * that are potentially unique for every watch.
 */
static void RunTickTime( hsm_status status, void * user_data )
{
    // our context is the watch object
    Watch* watch= ((WatchContext*)status->ctx)->watch;
    // our event is the tick event
    TickEvent * tick= ((TickEvent*)status->evt);
    // tick by this much time
    TickTime ( watch, tick->time );
    // print the total time
    printf("%d,", watch->elapsed_time );
}


//---------------------------------------------------------------------------
static hsm_bool MatchEvent( hsm_status status, WatchEvents evt )
{
    return status->evt->type == evt;
}

#define IfEvent( val ) hsmIfUD( (hsm_callback_guard_ud) MatchEvent, (void*) val )

//---------------------------------------------------------------------------
/**
 * the watch chart can be used by multiple machines at the same time
 */
hsm_state buildWatchChart() 
{
    int id= hsmBegin( "Active",0 );
    {
        hsmOnEnter( ActiveStateEnter );   // active state enter resets the timer
            // if the user presses "reset" 
            // no matter which state we're in, transition to self, that means: 
            // reset the time ( via enter ), and enter initial state ( stopped )
            IfEvent( WATCH_RESET_PRESSED ); 
            hsmGoto( "Active" );
            
        // the first sub-state entered is the first state listed
        // in this case the first thing active does is enter 'stopped'
        hsmBegin( "Stopped",0 );
        {
            IfEvent( WATCH_TOGGLE_PRESSED );
            hsmGoto( "Running" );
        }
        hsmEnd();
        hsmBegin( "Running",0 );
        {
            IfEvent( WATCH_TOGGLE_PRESSED );
            hsmGoto( "Stopped" );

            // on tick events, update the watch timer
            IfEvent( WATCH_TICK );
            hsmRunUD( RunTickTime, 0 );
        }
        hsmEnd();
    }
    hsmEnd();
    return hsmResolveId(id);
}

//---------------------------------------------------------------------------
/**
 * watch_builder
 * run a stop watch using user input
 */
//---------------------------------------------------------------------------
int watch_builder( int argc, char* argv[] )
{   
    // the builder needs an early call to initialize its internals
    if (hsmStartup()) {
        hsm_context_machine_t machine;
        Watch watch;
        WatchContext ctx= { 0, 0, &watch };

        // declare a statemachine, pass our watch context data
        hsm_machine hsm= HsmMachineWithContext( &machine, &ctx.ctx );
        hsm_state watch_chart= buildWatchChart();


        printf( "HsmBuilder's Stop Watch Sample.\n"
            "Keys:\n"
                "\t'1': reset button\n"
                "\t'2': generic toggle button\n" );
    
        // start the machine, and some first state
        HsmStart( hsm, watch_chart );

        // while the statemachine is still running
        while ( HsmIsRunning( hsm ) ) {
            // get a key from the keyboard
            int ch= PlatformGetKey();
            // turn a '1' into the reset button,
            // turn a '2' into the toggle button
            WatchEvents events[]= { WATCH_RESET_PRESSED, WATCH_TOGGLE_PRESSED };
            int index= ch-'1';
            if ((index >=0) && (index < sizeof(events)/sizeof(WatchEvents))) {
                // send the event to the state machine
                // one of the handler functions will get called as a result
                // ( for example: StoppedStateEvent. ) 
                WatchEvent evt= { events[index] };
                HsmSignalEvent( hsm, &evt );
                printf(".");
            }
            else {
            // send a tick to the machine
            // we send this regardless of whether we think the watch is running or stopped.
            // the logic of the statemachine internally knows which states are currently active 
            // and sends the event to the appropriate state. noting (in the code above) that only 
            // RunningStateEvent has code does anything when it hears the tick event.
                TickEvent tick= { WATCH_TICK, 1 };
                HsmSignalEvent( hsm, &tick.core );
                PlatformSleep(500);
            }
        };
    }
    return hsmShutdown();
}

