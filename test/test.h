/**
 * @file test.h
 *
 * \internal
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __TEST_H__
#define __TEST_H__

#include <hsm/hsm_machine.h>

//---------------------------------------------------------------------------
typedef struct hsm_event_rec CharEvent;
struct hsm_event_rec {
    char ch;
};

//---------------------------------------------------------------------------
/**
 * TestEventSequence
 *
 * @param hsm machine to test
 * @param seq array of strings
 *
 * seq is a null-terminated string in the format: "expected output", "input", "expected output", ... NULL
 */
hsm_bool TestEventSequence( hsm_machine hsm, hsm_state first, const char ** seq );

// a state with nothing to do
#define EMPTY_STATE( s,p,i ) \
    HSM_STATE( s, p, i ); \
    hsm_state s##Event( hsm_status status ) { return NULL; }


#endif // #ifndef __TEST_H__
