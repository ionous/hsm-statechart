/**
 * @file samek_plus.h
 *
 * Define a sequence of events and states for the samek's example statemachine from page 95 of Practical Statecharts.
 *
 * A limitation of samek's original test is that there are no leaf states that are also siblings. 
 * That means there are no transitions where the source of the transition is higher than the lca of current and target.
 * This test, therefore, adds state s12 and event 'i'. 
 *
 * When current is 's11', and 'i' fires, 's0' becomes the source of a transition to 's12'.
 * The lca of current and target will be s1, and -- while normally that's important -- 
 * based on the spec, since source is *higher* than lca, we expect the 'lca' to be ignored. 
 * We expect to exit all the way up to the source 's0', then init down into 's12'.

 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __SAMEK_PLUS_H__
#define __SAMEK_PLUS_H__

/*
Samek p.95
  s0:
    - e >> s211.
    - i >> s12.                         # the extra event
    s1:
      - a >> s1.                        # self-transition to s1
      - b >> s11.
      - c >> s2.
      - d >> s0.
      - f >> s211.
      s11:
        - g >> s211.
        - h [ foo ] / (foo = 0);
      s12:                              # the extra state
    s2:
      - c >> s1.
      - f >> s11.
      s21:
        - b >> s211.
        - h [ !foo ] / (foo = 1); >> s21;
        s211:
        - d >> s21.
        - g >> s0.
*/
const char ** SamekPlusSequence();

#endif // #ifndef __SAMEK_PLUS_H__
