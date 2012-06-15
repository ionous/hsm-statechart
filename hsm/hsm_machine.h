/**
 * @file hsm_machine.h
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

#include "hsm_forwards.h"

/**
 * The statemachine object. 
 * Not meant to be manipulated directly.
 */
struct hsm_machine
{
    /**
     * inner-most state currently active
     * null until HsmStart() called
     * @see  HsmIsInState
     */
    hsm_state current;

    /**
     * the machine's context stack 
     * ( if you want one )
     */
    hsm_context_stack_t* stack;

    /**
     * called whenever the machine doesnt handle an event
     * by default this is null
     */
    hsm_callback_unhandled_event on_unhandled_event;
};

/**
 * Initialize a statemachine to its default values
 *
 * @param hsm Machine to initialize.
 * @param stack Context stack used for per state instance data
 * @param popped_callback Called every time a piece of context data gets popped due to state exit.
 */
hsm_machine_t* HsmMachine( hsm_machine_t * hsm, struct hsm_context_stack* stack, hsm_callback_context_popped  popped_callback );

/**
 * Start a machine.
 *
 * @param hsm The machine to start. 
 * @param context Optional context for the entire machine.
 * @param state The first state to move to.
 * @return HSM_FALSE on error ( ex. the machine was already started )
 */
hsm_bool HsmStart( hsm_machine_t* hsm, struct hsm_context* context, hsm_state staet );

/**
 * Send the passed event to the machine.
 * 
 * @param hsm The machine targeted. 
 * @param event A user defined event.
 *
 * The system will launch actions, trigger transitions, etc.
 * @return 'true' if handled
 */
hsm_bool HsmProcessEvent( hsm_machine_t* hsm, struct hsm_event* event );

/**
 * Determine if a machine has been started, and has not reached a terminal, nor an error state.
 *
 * @param hsm
 * @return HSM_TRUE if running
 */
hsm_bool HsmIsRunning( hsm_machine_t* hsm );

/**
 * Traverses the active state hierarchy to determine if hsm is possibly in the passed state.
 *
 * @param hsm
 * @param state
 */
hsm_bool HsmIsInState( hsm_machine_t* hsm, hsm_state state );

/**
 * A machine in the terminal state has deliberately killed itself.
 * 
 * @return the globally shared terminal pseduo-state.
 */
hsm_state HsmStateTerminated();

/**
 * Pseudo state for when a machine has inadvertently killed itself. 
 *
 * @return the globally shared error pseduo-state.
 */
hsm_state HsmStateError();

/**
 * Token state that can be used by event handler functions to indicate the event was handled
 * and no state transition is required.
 *
 * @return a globally "its okay" token.
 */
hsm_state HsmStateHandled();

/**
 * Pseudo state for use with the HSM_STATE macros to represent the outer most state
 * @return the globally shared top most token.
 */
hsm_state HsmTopState();

/**
 * Pseudo state for use with the HSM_STATE macros to represent no initial state
 * @return the globally shared top most token.
 */
hsm_state HsmNull();


#endif // #ifndef __HSM_MACHINE_H__
