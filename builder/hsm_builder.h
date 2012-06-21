/**
 * @file hsm_builder.h
 *
 * Alternate method for declaring and defining states.
 *
 * The builder is a layer on top of the core hsm statemachine code.
 * It is not necessary to use, or even include, the builder to use hsm-statechart.
 *
 * \internal
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
/*
 * proper reuse of builder is to rebuild not share states
 * ie. function() { hsmState(...): }
 * call it twice to embed the state machine twice, 
 * dont try to reuse the name it returns in multiple places
 */
#pragma once
#ifndef __HSM_BUILDER_H__
#define __HSM_BUILDER_H__


/**
 * Action callback.
 *
 * @param hsm The #hsm_machine processing the event that trigged the action.
 * @param ctx Context object returned by the state enter callback.
 * @param evt Event which triggered the action.
 */
typedef void(*hsm_callback_action)( hsm_machine hsm, hsm_context ctx, hsm_event evt );

/**
 * Guard callback.
 *
 * @param hsm The #hsm_machine processing the event that trigged the guard.
 * @param ctx Context object returned by the state enter callback.
 * @param evt Event which triggered the action.
 */
typedef hsm_bool(*hsm_callback_guard)( hsm_machine hsm, hsm_context ctx, hsm_event evt );

/**
 * Builder initialization.
 * <b>Must</b> be called before the very first 
 */
void hsmStartup();

/**
 * Builder shutdown.
 * Free all internally allocated memory.
 * All states are freed.
 */
void hsmShutdown();

/**
 * Declare a new state.
 *
 * @param name Name relative to enclosing state; if no enclosing state, assumed to be the top state of a machine.
 *
 * @return int opaque handle to an internal name
 *
 * This function is idempotent(!) 
 *
 * If you call it with the same name, in the same begin/end scope, in the same run of the app, you will get the same value back out. 
 * The value is *not* gaurenteed to be the same across different launches of the same application.
 * 
 * Names are combined using "::" as per the statechart spec. ex.: Parent::Child::Leaf
 *
 * What this all means is you cannot have two top level states both called "Foo" in your application.
 * You can however reuse states by within a statemachine provided they have different parents: 
 * ( ie. { Outer: { InnerA: Foo }  { InnerB: Foo } } is okay )
 */
int hsmState( const char * name );


/**
 * Define an already declared (or referenced) state.

 * Every hsmBegin() must, eventually, be paired with a matching hsmEnd(),
 * Until then, all operations, including calls including hsmState() are considered owned by this state.
 *
 * @param state A state id returned by hsmState or hsmRef.
 * @return The same state id that was passed in.
 *
 * @see hsmState, hsmEnd
 */
int hsmBegin( int state );

/**
 * Specify a callback for state entry
 *
 * @param entry
 */
void hsmOnEnter( hsm_callback_enter entry );

/**
 * Event handler initialization.
 * Assuming that the first member of event structure is an int ( ex. a function, a string pool pointer, an enum )
 * trigger the state's event handler when that event structure member equals the passed value.
 *
 * @param eventInt
 */
void hsmOni( int eventInt );

/**
 * The event handler being declared should transition to another state.
 * 
 * @param state The id of a state returned by hsmState() or hsmRef() to transition to. 
 *
 * @see hsmState, hsmEnd
 */
void hsmGoto( int state );        

/**
 * The event handler being declared should run the passed action.
 * @param action The action to run.
 */
void hsmRun( hsm_callback_action action );

/**
 * Pairs with 
 * @see hsmBegin()
 */
void hsmEnd();


#endif // #ifndef __HSM_BUILDER_H__
