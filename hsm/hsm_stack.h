/**
 * @file hsm_stack.h
 *
 * Internal stack manipulation routines used by hsm_machine
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __HSM_STACK_H__
#define __HSM_STACK_H__

//---------------------------------------------------------------------------
/**
 * Add a new context to the stack.
 *
 * @param stack Can be NULL.
 * @param context The new context. NULL is legal, but not recommended.
 * @note The current implementation uses a linked list of pointers, so handling NULL is necessary or the list would break.
 * currently, therefore, a context of NULL means "reuse the parent context", but it's possible this behavior may change in the future.
 */
void HsmContextPush( hsm_context_stack_t* stack, hsm_context_t* context );

/**
 * Remove the most recently added context.
 *
 * @param stack Stack to pop from. Can be NULL.
 */
void HsmContextPop( hsm_context_stack_t* stack );

//---------------------------------------------------------------------------
/**
 * Structure to traverse a context stack.
 *
 * Iteration happens in the order of event processing:
 * moving from the most leaf state to the top
 * that is: from newest pushed to oldest.
 */
struct hsm_context_iterator 
{
    /**
     * pointer the the machine's stack
     */
    hsm_context_stack_t *stack;
    /**
     * pointer to the current unique context data
     */
    hsm_context_t *context;
    /**
     * index of depth into the stack
     */
    int sparse_index;
};

/**
 * Initialize the iterator to the most recent state's context.
 */
void HsmContextIterator( hsm_context_stack_t *, hsm_context_iterator_t* );

/**
 * Advance the iterator to the parent state's context, return the parent context.
 */
hsm_context_t* HsmParentContext( hsm_context_iterator_t* );


#endif // #ifndef __HSM_STACK_H__
