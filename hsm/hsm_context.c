/**
 * hsm_context.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "hsm_context.h"
#include "hsm_stack.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

//---------------------------------------------------------------------------
// HsmContext
//---------------------------------------------------------------------------
hsm_context_t* HsmContext( hsm_context_t * ctx )
{
    assert( ctx );
    if (ctx) {
        ctx->parent= NULL;
    }        
    return ctx;
}

//---------------------------------------------------------------------------
// HsmContextStack
//---------------------------------------------------------------------------
hsm_context_stack_t* HsmContextStack( hsm_context_stack_t* stack, hsm_callback_context_popped on_popped )
{
    if(stack) {
        memset(stack, 0, sizeof(hsm_context_stack_t));
        stack->on_popped= on_popped;
    }        
    return stack;
}

//---------------------------------------------------------------------------
void HsmContextPush( hsm_context_stack_t* stack, hsm_context_t * ctx )
{
    if (stack) {
        // push if its a unique context:
        // (NULL means 'duplicate the previous context')
        if (ctx && (stack->context != ctx)) {
            ctx->parent    = stack->context;
            stack->context = ctx;
            stack->presence |= ( 1<< stack->count );
        }
        // regardless alway update the count
        // the presence bits start at zero, so presence[count] by default ==0
        ++stack->count;
    }
}

// ---------------------------------------------------------------
void HsmContextPop( hsm_context_stack_t* stack )
{
    hsm_context_t* prev;
    if (stack && stack->count > 0) {
        // get the presence tester
        hsm_uint32 bit= (1 << --stack->count);
        // was that a unique piece of data in that spot
        if ((stack->presence & bit) !=0) {
            // clear that bit
            stack->presence &= ~bit;
            // get the most recent thing pushed
            prev= stack->context;
            // and remove it 
            if (prev) {
                stack->context= prev->parent;
            }                
            // finally: let the user know
            if (stack->on_popped) {
                stack->on_popped( stack, prev );
            }
        }
    }
}

//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
void HsmContextIterator( hsm_context_stack_t* stack, hsm_context_iterator_t * it )
{
    assert( it );
    if (it) {
        if (!stack) {
            memset( it, 0, sizeof(hsm_context_iterator_t));
        }
        else {
            it->stack= stack;
            it->context = stack->context;
            it->sparse_index= stack->count-1; // start at *back* ( newest pushed )
        }
    }
}

/* ---------------------------------------------------------------------------
   |..|...|.. <- sparse stack:
   0123456789 <- 9 states deep in the presence; _top=9
   0  1   2   <- 3 contexts in the store; _store.size()=3
   to traverse the stack you start with _it=9, and the location of the last context
   whenever you cross a new set bit, you move the context location.
   an iterator across a series of bits also needs the source bits.
 * --------------------------------------------------------------------------- */ 
hsm_context_t* HsmParentContext( hsm_context_iterator_t* it )
{
    // does pointer possess potential parent presence? perhaps.
    if (it->sparse_index>0){
        hsm_uint32 bit= (1 << --it->sparse_index);
        if ((it->stack->presence & bit) !=0) {
            it->context= it->context->parent;
        }
    }
    return it->context;
}
