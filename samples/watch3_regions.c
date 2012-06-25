/**
 * @file watch1_named_events.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include <hsm/hsm_machine.h>    // the state machine
#include "watch.h"
#include "platform.h"

#include <stdlib.h>
#include <stdio.h>


//---------------------------------------------------------------------------
typedef enum watch_events WatchEvents;
enum watch_events 
{
    WATCH_RESET_PRESSED,
    WATCH_TOGGLE_PRESSED,
    WATCH_TICK,
};

//---------------------------------------------------------------------------
typedef struct hsm_event_rec WatchEvent;
struct hsm_event_rec 
{
    WatchEvents type;
};


//---------------------------------------------------------------------------
typedef struct tick_event TickEvent;
struct tick_event 
{
    WatchEvent core;
    int time;
};

//---------------------------------------------------------------------------
typedef struct watch_context WatchContext;
struct watch_context 
{
    hsm_context_t ctx;
    Watch * watch;
};

//---------------------------------------------------------------------------
typedef struct hsm_region_rec hsm_region_t;
typedef struct hsm_parallel_rec hsm_parallel_t;
typedef struct hsm_active_rec hsm_active_t;
/**
 * an active region.
 * note, multiple instances of the same region running at the same time
 * will need their own copies of this structure so they don't stomp each other's current state.
 */
struct hsm_active_rec
{
    hsm_context_t ctx;
    hsm_context_machine_t hsm;
    struct hsm_active_rec* next;
};


/**
 * 
 */
struct hsm_region_rec
{
    struct hsm_state_rec state;
    hsm_region_t * next;
    // TODO generate the actives dynamically; one per region 
    struct hsm_active_rec active;
};

/**
 * a group of regions is modeled as a state, 
 * with this structure as the context for that state
 */
struct hsm_parallel_rec
{
    hsm_region_t* first;
};


hsm_state HsmParallelEvent( hsm_status status );
hsm_context HsmParallelEnter( hsm_parallel_t* parallel, hsm_status status );
void HsmParallelExit( hsm_status status );
void HsmExit( hsm_machine hsm, hsm_event cause );

#define HSM_PARALLEL( Parent ) \
        hsm_context Parent##Parallel##Enter( hsm_status status ); \
        _HSM_STATE( Parent##Parallel, Parent, HsmParallelEvent, Parent##Parallel##Enter, HsmParallelExit, 0 ); \
        static hsm_parallel_t Parent##parallel= { 0 }; \
        hsm_context Parent##Parallel##Enter( hsm_status status ) { \
            return HsmParallelEnter( &(Parent##parallel), status ); \
        }

// FIX? if Parent() took at poiner of child, we could ignore it for all but parallels
// which would build an actual tree? then maybe we wouldnt need both parallel *and* region, just parallel
// we could have user code pass around raw fnc ptrs instead of the state descriptors
// going to wait till builder interface, and see how that works out there
#define HSM_REGION( State, Parent, Initial ) \
        hsm_state State##Lookup##Initial(); \
        hsm_state State##Lookup##0() { return 0; } \
        hsm_state State() { \
            static struct hsm_region_rec myinfo= { 0 }; \
            if (!myinfo.state.name) { \
                myinfo.state.name= #State; \
                myinfo.state.process= 0; \
                myinfo.state.enter= 0; \
                myinfo.state.exit= 0; \
                myinfo.state.initial= State##Lookup##Initial(); \
                myinfo.state.parent= Parent##Parallel(); \
                myinfo.state.depth= myinfo.state.parent->depth+1; \
                /*link in this region to the parallel*/ \
                myinfo.next= Parent##parallel.first;  \
                Parent##parallel.first= myinfo.next; \
            } \
            return &(myinfo.state); \
        }

//---------------------------------------------------------------------------
// declare the states just like watch1_enum_events.
//
HSM_STATE_ENTER( ActiveState3, HsmTopState, ActiveState3Parallel );

    // parallel declares a container
    // the container is just a special kind of state, with particular functions
    HSM_PARALLEL( ActiveState3 );

        HSM_REGION( DefaultRegion3, ActiveState3, StoppedState3 );
            HSM_STATE( StoppedState3, DefaultRegion3, 0 );
            HSM_STATE( RunningState3, DefaultRegion3, 0 );

        HSM_REGION( AutoDestructRegion3, ActiveState3, TimeBombState3 );
            HSM_STATE( TimeBombState3, AutoDestructRegion3, 0 );
        
hsm_context HsmParallelEnter( hsm_parallel_t* parallel, hsm_status status )
{
    // when we enter->ctx is the context of the previous state
    // in the real version we need to create an 'active' for every parallel
    // walk our list of regions and send them startups
    hsm_region_t *it;
    for (it= parallel->first; it; it=it->next) {
        HsmMachineWithContext( &(it->active.hsm), status->ctx );
        HsmStart( &(it->active.hsm.core), it->state.initial );
    }

    // finally: return our allocated regions data as context
    return &(parallel->first->active.ctx);
}

void HsmParallelExit( hsm_status status )
{
    hsm_active_t* it;
    for (it= ((hsm_active_t*)status->ctx); it; it=it->next) {
        // hsm->current is *us*, we exit all of the substates until they're unrolled back to the top
        while (  it->hsm.core.current != status->hsm->current ) {
            HsmExit( &(it->hsm.core), status->evt );
        }
    }
}

hsm_state HsmParallelEvent( hsm_status status )
{
    hsm_active_t* it;
    hsm_bool any_handled= 0;
    // all the regions get to try the event
    for (it= ((hsm_active_t*)status->ctx); it; it=it->next) {
        hsm_bool handled= HsmSignalEvent( &(it->hsm.core), status->evt );
        any_handled|= handled;
    }
    // if anyone handled then say so
    return any_handled ? HsmStateHandled() :  0;
}
            

//---------------------------------------------------------------------------
// and here's that callback now...
//
// on entry to the active state, it clears the watch's elapsed time
//
// the convention <StateName>Enter is used by the HSM_STATE_ENTER macro to designate the entry callback
//
hsm_context ActiveState3Enter( hsm_status status )
{
    Watch* watch=((WatchContext*)status->ctx)->watch;
    ResetTime( watch );
    return status->ctx;
}

//---------------------------------------------------------------------------
// this is the event handling function for active state
//
// the convention <StateName>Event is used by the HSM_STATE macros to indicate the handler function
//
hsm_state ActiveState3Event( hsm_status status )
{
    // by default this function does nothing....
    hsm_state ret=NULL;
    switch ( status->evt->type ) {
        // but, whenever the reset button is pressed...
        case WATCH_RESET_PRESSED:
            // it transitions to itself.
            ret= ActiveState3();
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
hsm_state StoppedState3Event( hsm_status status )
{
    // by default this function does nothing....
    // note: anything that we don't handle goes straight to our parent
    // neither Stopped nor Running handle 'RESET".
    // they both let ActiveStateEvent ( above ) do whatever it wants.
    hsm_state ret=NULL;
    switch (status->evt->type) {
        // but, when the 'toggle' button gets pressed....
        case WATCH_TOGGLE_PRESSED:
            // transition over to the running state
            ret= RunningState3();
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
// event handler for the running state
//
hsm_state RunningState3Event( hsm_status status )
{
    // by default this function does nothing....
    hsm_state ret=NULL;
    switch (status->evt->type) {
        // but, when the 'toggle' button gets pressed....
        case WATCH_TOGGLE_PRESSED:
            // transition back to the stopped state
            ret= StoppedState3();
        break;
        // also, when a 'tick' is sent, update our timer
        case WATCH_TICK:
        {
            // our context is the watch object
            Watch* watch=((WatchContext*)status->ctx)->watch;
            // our event is the tick event
            TickEvent* tick= (TickEvent*)status->evt;
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
hsm_state TimeBombState3Event( hsm_status status )
{
    hsm_state ret= NULL;
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
int watch3region_events( int argc, char* argv[] )
{   
    Watch watch;
    WatchContext ctx= { 0, &watch };
    hsm_context_machine_t machine;
    hsm_machine hsm= HsmMachineWithContext( &machine, &ctx.ctx );
    
    printf( "Stop Watch Sample with State Events.\n"
        "Keys:\n"
            "\t'1': reset button\n"
            "\t'2': generic toggle button\n" );
    
    HsmStart( hsm, ActiveState3() );

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
    return 0;
}
