/*
 * Copyright 2015 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef __BASE_STATE_MACHINE_H__
#define __BASE_STATE_MACHINE_H__

#include "base/list.h"

/* Values to indicate if state is final, not final, invalid */
#define STATE_FINAL			(1)
#define STATE_NOT_FINAL		(0)
#define STATE_NO_TRANSITION		(-1)

struct sm_data;

/* Allocates memory for arr based on the value of n states. */
struct sm_data *sm_init(size_t n);
/* Resets curr_state to start_state. */
void sm_reset_state(struct sm_data *sm);
void sm_add_nonfinal_state(struct sm_data *sm, int id);
void sm_add_final_state(struct sm_data *sm, int id);
void sm_add_start_state(struct sm_data *sm, int id);
/*
 * Given src and dst ids, it finds the corr state structures and adds curr
 * transition to list of valid transitions in the src structure
 */
void sm_add_transition(struct sm_data *sm, int src_id, int input, int dst_id);
/*
 * Given an input, it checks current state for valid transition with that
 * input.
 * If a transition is found, it updates output with the value of next state id.
 * Then, it updates curr state to next state.
 * If next state is final, it returns STATE_FINAL and resets state machine.
 * Else it returns STATE_NOT_FINAL.
 * If no valid transition is found, it returns STATE_NO_TRANSITION
 */
int sm_run(struct sm_data *sm, int input, int *output);

#endif /* __BASE_STATE_MACHINE_H__ */
