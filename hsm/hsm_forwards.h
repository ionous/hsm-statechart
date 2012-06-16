/**
 * @file hsm_forwards.h
 *
 * helps unwind dependencies b/t hsm header definitions in a friendly way.
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

#include "hsm_types.h"

// meant to be defined to user code
typedef const struct hsm_event_rec *hsm_event;

// meant to be opaque to user code
typedef const struct hsm_state_rec *hsm_state;

// meant to be opaque to user code
typedef struct hsm_context_rec *hsm_context;

// meant to be opaque to user code
typedef struct hsm_machine_rec *hsm_machine;

#endif // #ifndef __HSM_FORWARDS_H__
