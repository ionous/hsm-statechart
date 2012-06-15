/**
 * @file hsm_state.h
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

/**
 * a state's incoming event handler signature
 */
typedef hsm_state(*hsm_callback_process_event)( struct hsm_machine*, struct hsm_context*, struct hsm_event* );

/**
 * a state's action signature
 */
typedef void(*hsm_callback_action)( struct hsm_machine*, struct hsm_context*, struct hsm_event* );

/**
 * a state's guard signature
 */
typedef hsm_bool(*hsm_callback_guard)( struct hsm_machine*, struct hsm_context*, struct hsm_event* );

/**
 * a state's entry callback signature
 */
typedef struct hsm_context*(*hsm_callback_enter)( struct hsm_machine*, struct hsm_context*, struct hsm_event* );

/**
 * a state's exit callback signature
 */
typedef void(*hsm_callback_exit)( struct hsm_machine*, struct hsm_context*, struct hsm_event* );

/**
 * a state's info descriptor function signature
 */
typedef hsm_state(*hsm_state_fn)();

//---------------------------------------------------------------------------
/**
 * a state descriptor
 * http://dev.ionous.net/2012/06/state-descriptors.html
 */
struct hsm_state_rec
{
    /**
     * name of the state ( useful for debugging )
     */
    const char * name;

    /**
     * the state's event handler callback
     */
    hsm_callback_process_event process;
    
    /**
     * enter action handler
     */
    hsm_callback_enter enter; 

    /**
     * exit action handler
     */
    hsm_callback_exit exit;

    /**
     * default sub-state (if any) for this state
     */
    hsm_state_fn initial;

    /**
     * parent of this state
     */
    hsm_state parent;

    /**
     * distance to the root state
     * this->depth = this->parent->depth+1; 
     * root most state's depth == 0
     */
    int depth;
};

//---------------------------------------------------------------------------
/**
 * macro for defining a state info
 * see additional @verbinclude hsm_state.txt.
 *
 * @param state      User defined name for the state.
 * @param parent     a user defined state name, or HsmTopState.
 * @param process    Event handler function.
 * @param enter      Function to call on state enter.
 * @param exit       Function to call on state exit.
 */
#define _HSM_STATE( State, Parent, Process, Enter, Exit, Initial ) \
        hsm_state Parent(); \
        hsm_state_fn State##Lookup##Initial(); \
        hsm_state_fn State##Lookup##0() { return 0; } \
        hsm_state State() { \
            static struct hsm_state_rec myinfo= { 0 }; \
            if (!myinfo.name) { \
                myinfo.name= #State; \
                myinfo.process= Process; \
                myinfo.enter= Enter; \
                myinfo.exit= Exit; \
                myinfo.initial= State##Lookup##Initial(); \
                myinfo.parent= Parent(); \
                myinfo.depth= myinfo.parent->depth+1; \
            } \
            return &myinfo; \
        } \
        hsm_state_fn Parent##Lookup##State() { return State; }
 
/**
 * helper macro for defining an hsm_state ( aka a state ).
 * requires an event handler function: <state>Event
 *
 * @param state      User defined name for the state.
 * @param parent     HsmTopState or a previously declared user defined state name.
 * @param initial    First state this state should enter. can be NULL.
 */
#define HSM_STATE( state, parent, initial ) \
        hsm_state state##Event( struct hsm_context*, struct hsm_event* ); \
        _HSM_STATE( state, parent, state##Event, 0, 0, initial )

/**
 * helper macro for defining an hsm_state ( aka a state ).
 * requires an event handler function: <state>Event
 * requires an entry function        : <state>Enter
 *
 * @param state      User defined name for the state.
 * @param parent     a user defined state name, or HsmTopState.
 * @param initial    First state this state should enter. can be NULL.
 */
#define HSM_STATE_ENTER( state, parent, initial ) \
        hsm_state state##Event( struct hsm_context*, struct hsm_event* ); \
        struct hsm_context* state##Enter( struct hsm_context*, struct hsm_event* ); \
        _HSM_STATE( state, parent, state##Event, state##Enter, 0, initial );

/**
 * helper macro for defining an hsm_state ( aka a state ).
 * requires an event handler function: <state>Event
 * requires an entry function        : <state>Enter
 * requires an exit function         : <state>Exit
 *
 * @param state      User defined name for the state.
 * @param parent     HsmTopState or a previously declared user defined state name.
 * @param initial    First state this state should enter. can be NULL.
 */
#define HSM_STATE_ENTERX( state, parent, initial ) \
        hsm_state state##Event( struct hsm_context*, struct hsm_event* ); \
        struct hsm_context* state##Enter( struct hsm_context*, struct hsm_event* ); \
        void state##Exit ( struct hsm_context*, struct hsm_event* ); \
        _HSM_STATE( state, parent, state##Event, state##Enter, state##Exit, initial );


#endif // #ifndef __HSM_INFO_H__

