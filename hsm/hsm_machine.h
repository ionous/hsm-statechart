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

#include "hsm_info.h"
#include "hsm_stack.h"
#include "hsm_state.h"

typedef struct hsm_machine_rec hsm_machine_t;
typedef struct hsm_context_machine_rec hsm_context_machine_t;

// give the lower 16 to user flags
#define HSM_FLAGS_CTX      (1<<16)
//#define HSM_FLAGS_INFO   (1<<17)   // flags per thing to log?
//#define HSM_FLAGS_REGION (1<<18)

//---------------------------------------------------------------------------
/**
 * The statemachine object. 
 *
 * 1. Initialize with HsmMachine()
 * 2. Start the machine with HsmStart()
 * 3. Send events with HsmProcessEvent()
 *
 * Not meant to be manipulated directly.
 */
struct hsm_machine_rec
{
    /**
     * per state machine flags
     */
    hsm_uint32 flags;
    
    /**
     * inner-most state currently active
     * null until HsmStart() called
     * @see HsmIsInState
     */
    hsm_state current;
};

/**
 * Extends hsm_machine_rec with a context stack
 */
struct hsm_context_machine_rec
{
    hsm_machine_t core;
    hsm_context_stack_t stack;
};


//---------------------------------------------------------------------------
/**
 * Initialize a statemachine to its default values
 *
 * @param hsm Machine to initialize.
 * @param stack Context stack used for per state instance data.
 */
hsm_machine HsmMachine( hsm_machine_t* hsm );

/**
 * Initialize a statemachine with a context stack to its default values
 *
 * @param hsm Machine to initialize.
 * @param context Optional context for the entire machine.
 * @param info Callbacks for listening to machine internals.
 */
hsm_machine HsmMachineWithContext( hsm_context_machine_t* hsm, hsm_context ctx );

/**
 * Start a machine.
 *
 * @param hsm The machine to start. 
 * @param state The first state to move to.
 * @return HSM_FALSE on error ( ex. the machine was already started )
 */
hsm_bool HsmStart( hsm_machine hsm, hsm_state state );

/**
 * Send the passed event to the machine.
 * 
 * @param hsm The machine targeted. 
 * @param event A user defined event.
 *
 * The system will launch actions, trigger transitions, etc.
 * @return 'true' if handled
 */
hsm_bool HsmProcessEvent( hsm_machine hsm, hsm_event event );

/**
 * Determine if a machine has been started, and has not reached a terminal, nor an error state.
 *
 * @param hsm
 * @return HSM_TRUE if running
 */
hsm_bool HsmIsRunning( const hsm_machine hsm );

/**
 * Traverses the active state hierarchy to determine if hsm is possibly in the passed state.
 *
 * @param hsm
 * @param state
 */
hsm_bool HsmIsInState( const hsm_machine hsm, hsm_state state );

/**
 * A machine in a final state has deliberately killed itself.
 * 
 * @return the globally shared final (pseduo) state.
 */
hsm_state HsmStateFinal();

/**
 * Token state for when a machine has inadvertently killed itself. 
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
 * Token state for use with the HSM_STATE macros to represent the outer most state
 * @return the globally shared top most token.
 */
hsm_state HsmTopState();


#endif // #ifndef __HSM_MACHINE_H__
