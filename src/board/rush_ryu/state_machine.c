/*
 * Copyright 2014 Google Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#include <assert.h>
#include <libpayload.h>

#include "board/rush_ryu/state_machine.h"

/*
 * Each state is represented by:
 * Unique id provided by caller
 * Whether this is final state
 * List of all valid transitions from this state
 */
struct sm_state {
	int id;
	int is_final;
	ListNode transitions;
};

/*
 * Each transition is represented by:
 * Input that it takes
 * Dest state because of this transition
 * Pointer to next valid transition at source state
 */
struct sm_transition {
	int input;
	struct sm_state *next;
	ListNode list_node;
};

/*
 * State machine is represented by:
 * Start state
 * Current state of the machine
 * Array containing pointers to each state machine structure(arr)
 * Used slots in state machine arr
 * Max slots available in state machine arr
 */
struct sm_data {
	struct sm_state *start_state;
	struct sm_state *curr_state;
	size_t used;
	size_t max;
	struct sm_state *arr[0];
};

/* Allocates memory for arr based on the value of n states */
struct sm_data *sm_init(size_t n)
{
	struct sm_data *sm = xzalloc(sizeof(*sm) +
				     n * sizeof(struct sm_state *));

	sm->max = n;

	return sm;
}

/* Returns pointer to a state given its id. */
static struct sm_state *sm_find_state(struct sm_data *sm, int id)
{
	int i = 0;

	for(; i < sm->used; i++) {
		if (sm->arr[i]->id == id)
			return sm->arr[i];
	}

	return NULL;
}

/* Resets curr_state to start_state. */
void sm_reset_state(struct sm_data *sm)
{
	sm->curr_state = sm->start_state;
}

/* Sets start_state given id of the state. */
static void sm_set_start_state(struct sm_data *sm, int id)
{
	struct sm_state *state = sm_find_state(sm, id);
	sm->start_state = state;
	sm_reset_state(sm);
}

/* Allocates memory for new state and adds to arr in state machine. */
static void sm_add_state(struct sm_data *sm, int id, int is_final)
{
	assert(sm->used < sm->max);

	/* State already exists in state machine table */
	if (sm_find_state(sm, id))
		return;

	struct sm_state *state = xzalloc(sizeof(*state));
	state->id = id;
	state->is_final = is_final;

	sm->arr[sm->used] = state;
	sm->used++;
}

void sm_add_nonfinal_state(struct sm_data *sm, int id)
{
	sm_add_state(sm, id, STATE_NOT_FINAL);
}

void sm_add_final_state(struct sm_data *sm, int id)
{
	sm_add_state(sm, id, STATE_FINAL);
}

void sm_add_start_state(struct sm_data *sm, int id)
{
	sm_add_state(sm, id, STATE_NOT_FINAL);
	sm_set_start_state(sm, id);
}

/*
 * Given src and dst ids, it finds the corr state structures and adds curr
 * transition to list of valid transitions in the src structure
 */
void sm_add_transition(struct sm_data *sm, int src_id, int input, int dst_id)
{
	struct sm_state *src = sm_find_state(sm, src_id);
	struct sm_state *dst = sm_find_state(sm, dst_id);

	assert (src && dst);

	struct sm_transition *tran = xzalloc(sizeof(*tran));
	tran->input = input;
	tran->next = dst;

	list_insert_after(&tran->list_node, &src->transitions);
}

/*
 * Given an input, it checks current state for valid transition with that
 * input.
 * If a transition is found, it updates output with the value of next state id.
 * Then, it updates curr state to next state.
 * If next state is final, it returns STATE_FINAL and resets state machine.
 * Else it returns STATE_NOT_FINAL.
 * If no valid transition is found, it returns STATE_NO_TRANSITION
 */
int sm_run(struct sm_data *sm, int input, int *output)
{
	assert(sm->curr_state && sm->start_state);

	struct sm_state *curr = sm->curr_state;

	struct sm_transition *tran;

	list_for_each(tran, curr->transitions, list_node) {
		if (tran->input != input)
			continue;

		sm->curr_state = tran->next;
		*output = sm->curr_state->id;
		if (sm->curr_state->is_final == STATE_FINAL) {
			sm_reset_state(sm);
			return STATE_FINAL;
		} else
			return STATE_NOT_FINAL;
	}

	return STATE_NO_TRANSITION;
}
