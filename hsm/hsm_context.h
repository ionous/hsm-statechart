/**
 * @file hsm_context.h
 *
 * Support for optional per-state instance data.
 *
 * \internal
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __HSM_CONTEXT_H__
#define __HSM_CONTEXT_H__

#include "hsm_forwards.h"

typedef struct hsm_context_rec hsm_context_t;
typedef struct hsm_context_stack_rec hsm_context_stack_t;
typedef struct hsm_context_stack_rec *hsm_context_stack;

//---------------------------------------------------------------------------
/**
 * A per state instance context object.
 * You can "derive" from this structure by making the first member of your own structure
 *
 * @note lifetime must be >= duration of its associated state(s).
 */
struct hsm_context_rec
{
    /**
     * @internal: pointer to next highest unique context
     */
    hsm_context_t* parent;
};

/**
 * Initializes the context structure
 * ( not strictly necessary )
 */
hsm_context HsmContext( hsm_context_t * );

//---------------------------------------------------------------------------
/**
 * Per state machine instance manager of context data.
 *
 * note: the most effienct implementation depends on the sparsity of the stack
 *  if it's very sparse: a list of (context,depth) tuples might be best
 *  if it's very full: an array of [context,...] with blank elements(*) might be best
 *  flagging is somewhere in between, though, all methods are probably fine for most people.
 */
struct hsm_context_stack_rec
{
    /**
     * most recently added unique context pointer
     */
    hsm_context context;

    /**
     * total number of pushes that have occured
     * ( always >= the depth of the statemachine's deepest state )
     */
    hsm_uint16 count;

    /**
     * bit flags for whether a push added unique data
     */
    hsm_uint16 presence;
};

//---------------------------------------------------------------------------
/**
 * Resets the hsm_context_stack structure.
 * @param stack Stack to initialize.
 * @return the stack passed in.
 */
hsm_context_stack HsmContextStack( hsm_context_stack_t* stack );

#endif // __HSM_CONTEXT_H__
