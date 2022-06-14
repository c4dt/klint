#pragma once

#include "os/time.h"

#include <stdbool.h>
#include <stddef.h>

//@ #include "proof/ghost_map.gh"

struct index_pool;

//@ predicate poolp(struct index_pool* pool, size_t size, time_t expiration_time, list<pair<size_t, time_t> > items);
//@ fixpoint bool pool_young(time_t time, time_t expiration_time, size_t k, time_t v) { return time < expiration_time || time - expiration_time <= v; }

/**
 * @brief Allocates an index pool of the given size with the given expiration time.
 *
 * @param size
 * @param expiration_time time frame after which a given used index expires
 * @return struct index_pool*
 */
struct index_pool* index_pool_alloc(size_t size, time_t expiration_time);
/*@ requires size * sizeof(time_t) <= SIZE_MAX; @*/
/*@ ensures poolp(result, size, expiration_time, nil); @*/
/*@ terminates; @*/

/**
 * @brief  Tries to borrow an index from the pool given the current time.
Iff an index is available in the pool, either free or expired, borrows one and returns it as out_index
 *
 * @param pool
 * @param time time at which we want to borrow an index
 * @param out_index stores the index value iff the borrow was successful
 * @param out_used set to true iff the index expired.
 * @return true index borrow was successful
 * @return false index borrow failed
 */
bool index_pool_borrow(struct index_pool* pool, time_t time, size_t* out_index, bool* out_used);
/*@ requires poolp(pool, ?size, ?exp_time, ?items) &*&
	     time != TIME_MAX &*&
	     *out_index |-> _ &*&
	     *out_used |-> _; @*/
/*@ ensures *out_index |-> ?index &*&
	    *out_used |-> ?used &*&
	    (length(items) == size ? (ghostmap_forall(items, (pool_young)(time, exp_time)) ? result == false
											   : (result == true &*& used == true))
				   : result == true) &*&
	    result ? poolp(pool, size, exp_time, ghostmap_set(items, index, time)) &*&
		     index < size &*&
		     (used ? (ghostmap_get(items, index) == some(?old) &*&
			      false == pool_young(time, exp_time, index, old))
			   : (ghostmap_get(items, index) == none))
		   : poolp(pool, size, exp_time, items); @*/
/*@ terminates; @*/

/**
 * @brief "Refreshes" an index in the pool for the current time, i.e., sets its last-used time to the given time.
 *
 * @param pool
 * @param time
 * @param index
 */
void index_pool_refresh(struct index_pool* pool, time_t time, size_t index);
/*@ requires poolp(pool, ?size, ?exp_time, ?items) &*&
	     time != TIME_MAX &*&
	     index < size; @*/
/*@ ensures poolp(pool, size, exp_time, ghostmap_set(items, index, time)); @*/
/*@ terminates; @*/

/**
 * @brief Indicates whether the given index is used (as opposed to free or expired) at the given time.
 *
 * @param pool
 * @param time
 * @param index
 * @return true
 * @return false
 */
bool index_pool_used(struct index_pool* pool, time_t time, size_t index);
/*@ requires poolp(pool, ?size, ?exp_time, ?items); @*/
/*@ ensures poolp(pool, size, exp_time, items) &*&
	    switch (ghostmap_get(items, index)) {
	      case none: return result == false;
	      case some(t): return result == pool_young(time, exp_time, 0, t);
	    }; @*/
/*@ terminates; @*/

/**
 * @brief Attempts to get a parameter with the given name
 *
 * @param pool
 * @param index
 */
void index_pool_return(struct index_pool* pool, size_t index);
/*@ requires poolp(pool, ?size, ?exp_time, ?items) &*&
	     index < size; @*/
/*@ ensures poolp(pool, size, exp_time, ghostmap_remove(items, index)); @*/
/*@ terminates; @*/
