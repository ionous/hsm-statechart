/**
 * @file hsm_info.h
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */

#pragma once
#ifndef __HSM_INFO_H__
#define __HSM_INFO_H__

#include "hsm_forwards.h"
#include "hsm_state.h"

typedef struct hsm_info_rec hsm_info_t;

/**
 * hear about init states just after entering current but before entering dest
 * even though no 'init' pseudostate exists in hsm-statechart, 
 * the sequence in UML is enter first, take the init transition second.
 *
 * hsm->current has the new state
 * hsm->stack.context has most recently pushed context
 */
typedef void (*hsm_callback_initing)( const hsm_machine hsm, const hsm_state dest, void * user_data );

/**
 * hear about new states after they have just been entered
 *
 * hsm->current has the new state
 * hsm->stack.context has most recently pushed context
 */
typedef void (*hsm_callback_entered)( const hsm_machine hsm, const hsm_event evt, void * user_data );

/**
 * hear about states that are about to exit
 *
 * hsm->current has the exiting state
 * hsm->stack.context has the context (that may or may not be about to be popped )
 */
typedef void(*hsm_callback_exiting)( const hsm_machine hsm, const hsm_event evt, void * user_data );

/**
 * hear about unhandled events
 */
typedef void (*hsm_callback_unhandled_event)( const hsm_machine hsm, const hsm_event evt, void * user_data );

/**
 * hear about context objects that have just been popped
 * can be used to clean up memory allocated in enter
 */
typedef void (*hsm_callback_context_popped)( const hsm_machine hsm, hsm_context ctx, void * user_data );


//---------------------------------------------------------------------------
/**
 * notification of important... occurances... in the state machine
 *
 * by design, only provides information for things usercode can't sus out
 * for instance, no error callback, since user code can see that returned 
 * from process event and IsRunning
 */
struct hsm_info_rec
{
    /**
     * custom user data.
     */
    void * user_data;

    /**
     * called after just before a state enter occurs as a result of an initial state designation
     */
    hsm_callback_initing on_init;
    
    /**
     * called after every state's entry
     */
    hsm_callback_entered on_entered;

    /**
     * called just before every state's exit
     */
    hsm_callback_exiting on_exiting;
    
    /**
     * called whenever the machine doesnt handle an event
     */
    hsm_callback_unhandled_event on_unhandled_event;

    /**
     * called just after the context stack pops its data.
     */
    hsm_callback_context_popped on_context_popped;
};

//---------------------------------------------------------------------------
/**
 * install a new set of callbacks
 * currently there's just one set of callbacks for all threads; this is not locked or synchronized in anyway.
 * 
 * @param callbacks  The set of callbacks; individual callbacks can be NULL, as can the callbacks pointer itself.
 * @param old_callback The old set of callbacks. recommended you should record, and later restore the old callbacks.
 */
void HsmSetInfoCallbacks( hsm_info_t* callbacks, hsm_info_t* old_callbacks );

#endif // #ifndef __HSM_INFO_H__
