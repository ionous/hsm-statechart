/**
 * hsm_machine.h
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __HSM_MACHINE_H__
#define __HSM_MACHINE_H__

#include "hsm_types.h"

typedef struct hsm_machine hsm_machine_t;
typedef void (*hsm_callback_context_popped)( struct hsm_context_stack*, struct hsm_context* );
typedef void (*hsm_callback_unhandled_event)( struct hsm_machine*, struct hsm_event* );

struct hsm_machine
{
    /**
     * a shared event used to signal 'init'
     * by default this is null
     */
    struct hsm_event * init;

    /**
     * inner-most state currently active
     * null until HsmStart() called
     * @see  HsmIsInState
     */
    struct hsm_info * current;

    /**
     * the machine's context stack 
     * ( if you want one )
     */
    struct hsm_context_stack* stack;

    /**
     * called whenever the machine doesnt handle an event
     * by default this is null
     */
    hsm_callback_unhandled_event on_unhandled_event;
};

/**
 * Initialize a statemachine to its default values
 * @param machine Machine to initialize.
 * @param stack Context stack used for per state instance data
 * @param popped_callback Called every time a piece of context data gets popped due to state exit.
 */
hsm_machine_t* HsmMachine( hsm_machine_t *, struct hsm_context_stack* , hsm_callback_context_popped  );

/**
 * Start a machine.
 * @param context Optional context for the entire machine.
 * @param state The first state to move to.
 * @return HSM_FALSE on error ( ex. the machine was already started )
 */
hsm_bool HsmStart( hsm_machine_t*, struct hsm_context*, struct hsm_info * );

/**
 * send the passed event to the machine.
 * launch actions, trigger transitions, etc.
 * returns 'true' if handled
 */
hsm_bool HsmProcessEvent( hsm_machine_t*, struct hsm_event* );

/**
 * @param hsm
 * @return HSM_TRUE if running
 */
hsm_bool HsmIsRunning( hsm_machine_t* );

/**
 * traverses the hierarchy to determine if you are in the passed state
 */
hsm_bool HsmIsInState( hsm_machine_t*, struct hsm_info*  );

/**
 * a machine in the terminal state has deliberately killed itself.
 * @return the globally shared terminal pseduo-state.
 */
struct hsm_info* HsmStateTerminated();

/**
 * pseudo state for when a machine has inadvertently killed itself. 
 * @return the globally shared error pseduo-state.
 */
struct hsm_info* HsmStateError();

/**
 * token state that can be used by event handler functions to indicate the event was handled
 * and no state transition is required.
 *
 * @return a globally "its okay" token.
 */
struct hsm_info* HsmStateHandled();

/**
 * pseudo state for use with the HSM_STATE macros to represent the outer most state
 * @return the globally shared top most token.
 */
struct hsm_info* HsmTopState();

#endif // #ifndef __HSM_MACHINE_H__
