/**
 * proper reuse of builder is to rebuild not share states
 * ie. function() { hsmState(...): }
 * call it twice to embed the state machine twice, 
 * dont try to reuse the name it returns in multiple places
 */
#pragma once
#ifndef __HSM_BUILDER_H__
#define __HSM_BUILDER_H__


// requires hsm_state

/*
enum hsmHints
{
    HSM_SIBLING,
    HSM_CHILD,
    HSM_SELF
};
*/

/**
 * 
 */
void hsmStartup();
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
 * Declare a new state.
 *
 * @param scope Scope of the passed name.
 * @param name Name relative to the scope
 *
 * @return int opaque handle to an internal name reference
 *
 * The name underlying the returned value is idempotent, but the returned value only does the best it can.
 * The value returned may not be the same as hsmState, even when they refer to the same state.
 * The variability of the returned value occurs because the scope may not even exist when the ref is declared.
 */
// int hsmRef( enum hsmHints scope, const char * name );

/**
 * Define an already declared (or referenced) state.
 * @param state A state id returned by hsmState or hsmRef.
 *
 * Every hsmBegin() must, eventually, be paired with a matching hsmEnd(),
 * Until then, all operations, including calls including hsmState() are considered owned by this state.
 */
int hsmBegin( int state );

void hsmOnEnter( hsm_callback_enter );
void hsmOni( int );            

/**
 * Indicate a transition to another state.
 * @param state A state id returned by hsmState or hsmRef.
 */
void hsmGoto( int state );        
void hsmRun( hsm_callback_action );
void hsmEnd();

/**
 * operates on the *last* state closed with hsmEnd()
 * finialize interal references, results in an error if a transition or event references an undefined value
 */
//int hsmComplete(int);


//---------------------------------------------------------------------------
// function handling:
// enter, exit, etc.: how do we adapt these functions for typesafety?

//---------------------------------------------------------------------------
// events: 
// enums, strings, ???? what are they?
// perhaps the only way to make this generic is through a function match callback
// or through a "program" interface: tell the machine how to get at the event match
// there's simple: its just an object with a offset and a type (int,string...?), 
// medium: a function that returns that object
// complex: a function that does that matching.

//---------------------------------------------------------------------------
// naming:
// ways of handling goto, init, etc. for states that may or may not be created
// with class nesting, scope just works. more difficult here
// thinking strings and a crc table with names.building.like.this.
// perhaps some regex/glob like syntax if you dont care: *.substate
//
// string/crc tables, predeclared handles ( via hsmState ), offsets ( HSM_SELF, HSM_SIBLING )
// some sort of late binding interface: user defines a slot, later fixes it up


#endif // #ifndef __HSM_BUILDER_H__
