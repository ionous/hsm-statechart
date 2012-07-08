/**
 * @file samek_plus.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "samek_plus.h"

//---------------------------------------------------------------------------
/*
    SamekPlusSequence:

    the first line of sequence is the expected series of initial + enter due to HsmStart()
    for subsequent lines: the first string is the event to be sent, 
                      the remain strings are the expected results.
*/
static const char * Sequence1[] = {
    /* the expected sequence of initial events due to HsmStart() */
    "s0-ENTRY","s0-INIT","s1-ENTRY","s1-INIT","s11-ENTRY",

    /* s1 handles 'a' in a self-transition, init'ing back down to s11 */
    "a",    "s11-EXIT","s1-EXIT",
            "s1-ENTRY","s1-INIT","s11-ENTRY",

    /* s0 handles 'e' and directs entry down to s211 */
    "e",    "s11-EXIT","s1-EXIT",/*"s0-EXIT", 
                hsm-statechart, doesn't exit the source state, 
                unless the source state target itself.
            "s0-ENTRY",*/"s2-ENTRY","s21-ENTRY","s211-ENTRY",

    /* s0 handles 'e' and directs entry down to s211 again */
    "e",    "s211-EXIT","s21-EXIT","s2-EXIT",/*"s0-EXIT",
            "s0-ENTRY",*/"s2-ENTRY","s21-ENTRY","s211-ENTRY",

    /* unhandled */
    "a",    "EVT-a",        

    /* s21 handles 'h', f==0, guard passes, sets foo, self-transition to s21, inits down to s211 */
    "h",    "s211-EXIT","s21-EXIT",     
            "s21-ENTRY","s21-INIT","s211-ENTRY",

    /* s1 hears 'h', foo==1, guard filters, 'h' is unhandled */
    "h",    "EVT-h",        

    /* s211 handles 'g', directs to 's0', inits down to s11. */
    "g",    "s211-EXIT","s21-EXIT","s2-EXIT",/*"s0-EXIT",
            "s0-ENTRY",*/
            /* a little odd, but since we haven't exited s0, and it's not a leaf state, 
            we get an init without an immediately preceeding enter */
            "s0-INIT","s1-ENTRY","s1-INIT","s11-ENTRY",

    /* s11 handles 'h', clears foo */
    "h",

    /* s0 handles 'i', directs down to 's12' */
    "i",    "s11-EXIT","s1-EXIT",/*"s0-EXIT",
            "s0-ENTRY",*/"s1-ENTRY","s12-ENTRY",

    /* x isn't handled anywhere */
    "x",    "EVT-x",
    
    0
};

const char ** SamekPlusSequence()
{
    return Sequence1;
}