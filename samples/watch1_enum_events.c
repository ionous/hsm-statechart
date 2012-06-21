/**
 * @file watch1_named_events.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "hsm_machine.h"    // the state machine
#include "watch.h"          // watch object
#include "platform.h"       // console input/output

#include <stdlib.h>
#include <stdio.h>

typedef enum watch_events WatchEvents;

// declare an enum of all possible watch events.
// this is just one way to declare events.
// see also watch1_named_events.c
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
// declare the states:
//
// all of the HSM_STATE* macros take: 
//    1. the state being declared;
//    2. the state's parent state;
//    3. an initial child state to move into first.
// 
// in this example:
//   1. "ActiveState" is the root of this particular machine; 
//   2. its parent state is the system state "HsmTopState",
//   3. it lists 'StoppedState' as its last parameter inorder to move to that state first.
//
// "StoppedState" and "RunningState" are also states, 
// they are siblings and share the parent state: "ActiveState"
// 
// the only difference b/t HSM_STATE_ENTER and HSM_STATE is that HSM_ENTER 
// declares a callback that gets called whenever the state gets entered...
// 
HSM_STATE_ENTER( ActiveState, HsmTopState, StoppedState );
    HSM_STATE( StoppedState, ActiveState, 0 );
    HSM_STATE( RunningState, ActiveState, 0 );

//---------------------------------------------------------------------------
// and here's that callback now...
//
// on entry to the active state, it clears the watch's elapsed time
//
// the convention <StateName>Enter is used by the HSM_STATE_ENTER macro to designate the entry callback
//
hsm_context ActiveStateEnter( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    Watch* watch=((WatchContext*)ctx)->watch;
    ResetTime( watch );
    return ctx;
}

//---------------------------------------------------------------------------
// this is the event handling function for active state
//
// the convention <StateName>Event is used by the HSM_STATE macros to indicate the handler function
//
hsm_state ActiveStateEvent( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    // by default this function does nothing....
    hsm_state ret=NULL;
    switch ( evt->evt ) {
        // but, whenever the reset button is pressed...
        case WATCH_RESET_PRESSED:
            // it transitions to itself.
            ret= ActiveState();
            // self-transitions are used by statecharts to trigger exit and re-entry to the same state.
            // that means we we'll soon get our ActiveStateEnter function called
            // we will reset our watch timer, and b/c the macros above designated "StoppedState" as 
            // Active's first state, the statemachine will move to "Stopped"
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
// event handler for the stoppped state
//
hsm_state StoppedStateEvent( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
{
    // by default this function does nothing....
    // note: anything that we don't handle goes straight to our parent
    // neither Stopped nor Running handle 'RESET".
    // they both let ActiveStateEvent ( above ) do whatever it wants.
    hsm_state ret=NULL;
    switch (evt->evt) {
        // but, when the 'toggle' button gets pressed....
        case WATCH_TOGGLE_PRESSED:
            // transition over to the running state
            ret= RunningState();
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
// event handler for the running state
//
hsm_state RunningStateEvent( hsm_machine hsm, hsm_context ctx, const WatchEvent* evt )
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
 * the statechart that this sample implements:
 *
 *   Active:
 *    - enter / watch->reset_timer();
 *    - reset_button >> Active.
 *    Stopped:
 *      - toggle_button >> Running.
 *    Running:
 *      - toggle_button >> Stopped.
 *      - timer / watch->tick();
 *
 * for more info see:
 * http://code.google.com/p/hsm-statechart/wiki/StopWatch
 *
 */
int watch1_enum_events( int argc, char* argv[] )
{   
    hsm_context_machine_t machine;
    Watch watch;
    WatchContext ctx= { 0, &watch };
    
    // declare a statemachine, pass our watch context data
    hsm_machine hsm= HsmMachineWithContext( &machine, &ctx.ctx );

    printf( "Stop Watch Sample with State Events.\n"
        "Keys:\n"
            "\t'1': reset button\n"
            "\t'2': generic toggle button\n" );
    
    // start the machine, and the root state
    // ( really it can be any state, not just the root, but the watch chart wants the root state )
    HsmStart( hsm, ActiveState() );

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
            HsmProcessEvent( hsm, &evt );
            printf(".");
        }
        else {
        // send a tick to the machine
        // we send this regardless of whether we think the watch is running or stopped.
        // the logic of the statemachine internally knows which states are currently active 
        // and sends the event to the appropriate state. noting (in the code above) that only 
        // RunningStateEvent has code does anything when it hears the tick event.
            TickEvent tick= { WATCH_TICK, 1 };
            HsmProcessEvent( hsm, &tick.evt );
            PlatformSleep(500);
        }
    };
    return 0;
}
