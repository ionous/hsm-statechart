/**
 * @file hsm_info.h
 *
 * Callbacks for listening to internal statemachine processing.
 *
 * \internal
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */

#pragma once
#ifndef __HSM_INFO_H__
#define __HSM_INFO_H__

#include "hsm_forwards.h"
#include "hsm_state.h"

typedef struct hsm_info_rec hsm_info_t;

/**
 * Hear about states just before entering next state.
 * hsm-statechart supports initial states via the #HSM_STATE macros.
 *
 * @param hsm The #hsm_machine. note: hsm->current has the new state, and hsm->stack.context has most recently pushed context.
 * @param next The #hsm_state that's about to be entered.
 * @param user_data The #hsm_info_rec.user_data.
 * 
 * @note The sequence in UML is enter first, take the init transition second.
 *
 */
typedef void (*hsm_callback_initing)( const hsm_machine hsm, const hsm_state next, void * user_data );

/**
 * Hear about new states <i>just</i> after they have been entered.
 *
 * @param hsm The #hsm_machine. note: hsm->current has the new state, and hsm->stack.context has most recently pushed context.
 * @param evt The #hsm_event which triggered the exit.
 * @param user_data The #hsm_info_rec.user_data.
 */
typedef void (*hsm_callback_entered)( const hsm_machine hsm, const hsm_event evt, void * user_data );

/**
 * Hear about states that are about to exit
 *
 * @param hsm The #hsm_machine. note: hsm->current has the exiting state, and hsm->stack.context has the context (that may or may not be about to be popped ).
 * @param evt The #hsm_event which triggered the exit.
 * @param user_data The #hsm_info_rec.user_data.
 */
typedef void(*hsm_callback_exiting)( const hsm_machine hsm, const hsm_event evt, void * user_data );

/**
 * Hear about unhandled events.
 * @param hsm The #hsm_machine processing. 
 * @param evt The #hsm_event which no state has handled.
 * @param user_data The #hsm_info_rec.user_data.
 */
typedef void (*hsm_callback_unhandled_event)( const hsm_machine hsm, const hsm_event evt, void * user_data );

/**
 * Hear about context objects that have just been popped;
 * can be used to free memory if user code allocated memory for state instance data during a state's enter callback.
 *
 * @param hsm The #hsm_machine processing. 
 * @param ctx The #hsm_context which has just been removed from the stack.
 * @param user_data The #hsm_info_rec.user_data.
 *
 * @see hsm_callback_enter, HSM_STATE_ENTER
 */
typedef void (*hsm_callback_context_popped)( const hsm_machine hsm, hsm_context ctx, void * user_data );


//---------------------------------------------------------------------------
/**
 * Notification of important occurances in the state machine.
 *
 * By design hsm_info only provides information for situtations user code can't sus out via the hsm_machine.h interface.
 * For instance, there isn't a callback on a transition to #HsmStateError, since user code can already detect errors via
 * the return codes of HsmProcessEvent() and HsmIsRunning()
 */
struct hsm_info_rec
{
    /**
     * Custom user data.
     */
    void * user_data;

    /**
     * Called after just before a state enter occurs as a result of an initial state designation.
     */
    hsm_callback_initing on_init;
    
    /**
     * Called after every state's entry
     */
    hsm_callback_entered on_entered;

    /**
     * Called just before every state's exit
     */
    hsm_callback_exiting on_exiting;
    
    /**
     * Called whenever the machine doesnt handle an event
     */
    hsm_callback_unhandled_event on_unhandled_event;

    /**
     * Called just after the context stack pops its data.
     */
    hsm_callback_context_popped on_context_popped;
};

//---------------------------------------------------------------------------
/**
 * Install a new set of callbacks for spying on internal statemachine processing.
 * Currently there's just one set of callbacks for all threads; this is not locked or synchronized in anyway.
 * 
 * @param callbacks  The set of callbacks; individual callbacks can be NULL, as can the callbacks pointer itself.
 * @param old_callbacks The last set of callbacks passed to HsmSetInfoCallbacks. Its recommended that you should record, and later restore the old callbacks.
 */
void HsmSetInfoCallbacks( hsm_info_t* callbacks, hsm_info_t* old_callbacks );

#endif // #ifndef __HSM_INFO_H__
