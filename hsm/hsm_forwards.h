/**
 * hsm_types.h
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __HSM_FORWARDS_H__
#define __HSM_FORWARDS_H__

// not ideal, but helps with ansi-c compliance for gcc 
#include "hsm_types.h"

typedef struct hsm_context hsm_context_t;
typedef struct hsm_context_stack hsm_context_stack_t;
typedef struct hsm_context_iterator hsm_context_iterator_t;
typedef struct hsm_event hsm_event_t;
typedef struct hsm_machine hsm_machine_t;

typedef void (*hsm_callback_context_popped)( hsm_context_stack_t*, hsm_context_t* );
typedef void (*hsm_callback_unhandled_event)( hsm_machine_t*, hsm_event_t* );

#endif // #ifndef __HSM_FORWARDS_H__