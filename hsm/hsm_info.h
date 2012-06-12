/**
 * hsm_info.h
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

typedef struct hsm_info hsm_info_t;

/**
 * a state's incoming event handler signature
 */
typedef struct hsm_info*(*hsm_callback_process_event)( struct hsm_machine*, struct hsm_context*, struct hsm_event* );

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


//---------------------------------------------------------------------------
/**
 * a state descriptor
 * http://dev.ionous.net/2012/06/state-descriptors.html
 */
struct hsm_info
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
     * parent of this state
     */
    struct hsm_info * parent;

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
 *
 * @param state      User defined name for the state.
 * @param parent     HsmTopState or a previously declared user defined state name.
 * @param process    Event handler function.
 * @param enter      Function to call on state enter.
 * @param exit       Function to call on state exit.
 */
#define _HSM_STATE( name, parentname, process, enter, exit ) \
        struct hsm_info*name##Event( struct hsm_machine*, struct hsm_context*, struct hsm_event* );     \
        struct hsm_context* name##Enter( struct hsm_machine*, struct hsm_context*, struct hsm_event* );     \
        void name##Exit( struct hsm_machine*, struct hsm_context*, struct hsm_event* );     \
        static hsm_info_t* name() { \
            static struct hsm_info myinfo= { #name, process, enter, exit  }; \
            static int initialized=0;\
            if (!initialized) { \
                struct hsm_info* pinfo= parentname(); \
                myinfo.parent= pinfo;\
                myinfo.depth=pinfo->depth+1;\
                initialized=1;\
            }\
            return &myinfo; \
        } 

/**
 * macro for defining a state info.
 * requires an event handler function: <state>Event
 *
 * @param state      User defined name for the state.
 * @param parent     HsmTopState or a previously declared user defined state name.
 */
#define HSM_STATE( state, parent ) _HSM_STATE( state, parent, state##Event, 0, 0 );

/**
 * macro for defining a state info
 * requires an event handler function: <state>Event
 * requires an entry function        : <state>Enter
 *
 * @param state      User defined name for the state.
 * @param parent     HsmTopState or a previously declared user defined state name.
 */
#define HSM_STATE_ENTER( state, parent ) _HSM_STATE( state, parent, state##Event,state##Enter, 0 );

/**
 * macro for defining a state info
 * requires an event handler function: <state>Event
 * requires an entry function        : <state>Enter
 * requires an exit function         : <state>Exit
 *
 * @param state      User defined name for the state.
 * @param parent     HsmTopState or a previously declared user defined state name.
 */
#define HSM_STATE_ENTERX( state, parent ) _HSM_STATE( state, parent, state##Event, state##Enter, state##Exit );


#endif // #ifndef __HSM_INFO_H__
