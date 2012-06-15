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

// declare an enum of all possible watch events
// this is just one way to declare events
// see also watch1_named_events.c
enum watch_events {
    WATCH_RESET_PRESSED,
    WATCH_TOGGLE_PRESSED,
    WATCH_TICK,
};

// it's up to the user code to define the hsm_event structure
// again, this is just one way to declare events
// you could also wrap an another the event's of another framework if you wanted
typedef struct hsm_event WatchEvent;
struct hsm_event {
    WatchEvents evt;
};

// the tick event extends the basic watch event to add elapsed time
// you can have as many different kinds of event data as you want
typedef struct tick_event TickEvent;
struct tick_event {
    WatchEvent evt;
    int time;
};

// a context object it sent to every callback of the statemachine
// you always need to have an hsm_context_t member as the first element
// after that, it's up to you what you include.
typedef struct watch_context WatchContext;
struct watch_context {
    hsm_context_t ctx;
    Watch * watch;
};

//---------------------------------------------------------------------------
// declare the states:
// 
// "ActiveState" is the root of this particular machine; 
//  its parent state is the system state "HsmTopState"
//  it lists 'StoppedState' as its last parameter inorder to move to that state first
//
// "Stopped" and "Running" are children of "Active"
// 
// the only difference b/t HSM_STATE_ENTER and HSM_STATE is that HSM_ENTER 
// declares a callback for ActiveState that gets called whenever the state gets entered
// 
HSM_STATE_ENTER( ActiveState, HsmTopState, StoppedState );
    HSM_STATE( StoppedState, ActiveState, 0 );
    HSM_STATE( RunningState, ActiveState, 0 );

//---------------------------------------------------------------------------
// here's that callback mentioned now...
// all that the active state does is clear the watch timer
//
// the convention: <StateName>Enter is used by the HSM_STATE_ENTER macro
//
hsm_context_t* ActiveStateEnter( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    Watch* watch=((WatchContext*)ctx)->watch;
    ResetTime( watch );
    return ctx;
}

//---------------------------------------------------------------------------
// this is the event handling function for active state
//
// the convention: <StateName>Event is used by the HSM_STATE macros to indicate the handler function
//
hsm_state ActiveStateEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    // by default this function does nothing....
    hsm_state ret=NULL;
    switch ( evt->evt ) {
        // but, whenever the reset button is pressed...
        case WATCH_RESET_PRESSED:
            // transition to ourself:
            // self-transitions are used by statecharts to trigger exit and re-entry to the same state
            ret= ActiveState();
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
// event handler for the stoppped state
//
hsm_state StoppedStateEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    // by default this function does nothing....
    hsm_state ret=NULL;
    switch (evt->evt) {
        // but, when the 'toggle' button gets pressed....
        case WATCH_TOGGLE_PRESSED:
            // transition to the running state
            ret= RunningState();
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
// event handler for the running state
//
hsm_state RunningStateEvent( hsm_machine_t*hsm, hsm_context_t*ctx, WatchEvent* evt )
{
    // by default this function does nothing....
    hsm_state ret=NULL;
    switch (evt->evt) {
        // but, when the 'toggle' button gets pressed....
        case WATCH_TOGGLE_PRESSED:
            // transition back to the stopped state
            ret= StoppedState();
        break;
        // also, when a 'tick' is sent, update our timer
        case WATCH_TICK:
        {
            // our context is the watch object
            Watch* watch=((WatchContext*)ctx)->watch;
            // our event is the tick event
            TickEvent* tick= (TickEvent*)evt;
            // tick by this much time
            TickTime ( watch, tick->time );
            // print the total time
            printf("%d,", watch->elapsed_time );
            // indicate to the statemachine that we've take care of this event
            // this stops the event from being sent to our parent
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
    
    printf( "Stop Watch Sample with State Events.\n"
        "Keys:\n"
            "\t'1': reset button\n"
            "\t'2': generic toggle button\n" );
    
    // declare a statemachine
    // the "stack" allows us to pass context data to our callbacks
    HsmMachine( &hsm, &stack, NULL );

    // start the machine, pass our watch context data, and the root state
    // ( really it can be any state, not just the root, but the watch chart wants the root state )
    HsmStart( &hsm, &ctx.ctx, ActiveState() );

    // while the statemachine is still running
    while ( HsmIsRunning( &hsm ) ) {
        // get a key from the keyboard
        int ch= PlatformGetKey();
        // turn a '1' into the reset button,
        // turn a '2' into the toggle button
        WatchEvents events[]= { WATCH_RESET_PRESSED, WATCH_TOGGLE_PRESSED };
        int index= ch-'1';
        if ((index >=0) && (index < sizeof(events)/sizeof(WatchEvents))) {
            // send the event to the state machine
            // one of the handler functions, ( like for example, StoppedStateEvent ) will get called as a result
            WatchEvent evt= { events[index] };
            HsmProcessEvent( &hsm, &evt );
            printf(".");
        }
        else {
        // send a tick to the machine
        // we send this regardless of whether we think the watch is running or stopped.
        // the logic of the statemachine internally knows which states are currently active 
        // and sends the event to the appropriate state. noting (in the code above) that only 
        // RunningStateEvent has code does anything when it hears the tick event.
            TickEvent tick= { WATCH_TICK, 1 };
            HsmProcessEvent( &hsm, &tick.evt );
            PlatformSleep(500);
        }
    };
    return 0;
}
