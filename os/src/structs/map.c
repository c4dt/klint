#include "os/structs/map.h"

// !!! IMPORTANT !!! to verify, 'default_value_eq_zero' needs to be turned from a lemma_auto to a lemma in prelude_core.gh, see verifast issue 68
//#include "map-impl-pow2.c"
#include <stdbool.h>
#include <stdlib.h>

// TODO REMOVE ME
void map_impl_erase/*@ <kt> @*/(int* busybits, void** keyps,
                                unsigned* k_hashes, int* chns,
                                 void* keyp,
                                 unsigned hash, unsigned key_size, unsigned capacity);

#include "generic_ops.h"

//@ #include "proof/chain-buckets.gh"
//@ #include "proof/listexex.gh"
//@ #include "proof/mod-pow2.gh"
//@ #include "proof/modulo.gh"
//@ #include "proof/nth-prop.gh"
//@ #include "proof/stdex.gh"

struct os_map {
  void** kaddrs;
  int* busybits;
  unsigned* hashes;
  int* chains;
  uint64_t* values;
  uint64_t capacity;
  uint64_t key_size;
};

/*@
fixpoint void* map_item_addr(map_item item) { 
  switch(item) { 
    case map_item(addr, key, val):
      return addr;
  } 
}

fixpoint bool map_no_dups(list<map_item> items) {
  return distinct(items) && distinct(map(map_item_key, items));
}
@*/

/*@
  predicate key_opt_list(unsigned key_size, list<void*> kaddrs, list<int> busybits; list<option<list<char> > > key_opts) =
    switch(busybits) {
      case nil:
        return kaddrs == nil &*& key_opts == nil;
      case cons(h,t):
        return kaddrs == cons(?kaddrsh,?kaddrst) &*&
               key_opt_list(key_size, kaddrst, t, ?key_optst) &*&
               h == 0 ? key_opts == cons(none, key_optst) : ([0.25]chars(kaddrsh, key_size, ?key_optsh) &*& key_opts == cons(some(key_optsh), key_optst));
    };

  // NOTE: It's important that we say nothing about hashes of none keys, which is why this can't return a list<unsigned> (which would need revealing this info)
  fixpoint bool hash_list(list<option<list<char> > > key_opts, list<unsigned> hashes) {
    switch(key_opts) {
      case nil:
        return hashes == nil;
      case cons(h,t):
        return hashes != nil &&
               hash_list(t, tail(hashes)) &&
               (h == none || head(hashes) == hash_fp(get_some(h)));
    }
  }

  fixpoint bool has_given_hash_fp(int pos, unsigned capacity, pair<list<char>, nat> chain) {
    return pos == loop_fp(hash_fp(fst(chain)), capacity);
  }

  fixpoint bool key_chains_start_on_hash_fp(list<bucket<list<char> > > buckets, int pos, unsigned capacity) {
    switch(buckets) {
      case nil: return true;
      case cons(h,t):
        return switch(h) { case bucket(chains):
            return forall(chains, (has_given_hash_fp)(pos, capacity)) &&
                   key_chains_start_on_hash_fp(t, pos + 1, capacity);
          };
    }
  }

  predicate buckets_keys_insync(unsigned capacity, list<int> chains, list<bucket<list<char> > > buckets; list<option<list<char> > > key_opts) =
    chains == buckets_get_chns_fp(buckets) &*&
    true == buckets_ok(buckets) &*&
    true == key_chains_start_on_hash_fp(buckets, 0, capacity) &*&
    key_opts == buckets_get_keys_fp(buckets) &*&
    length(buckets) == capacity;

  predicate buckets_keys_insync_Xchain(unsigned capacity, list<int> chains, list<bucket<list<char> > > buckets, int start, int fin; list<option<list<char> > > key_opts) =
    chains == add_partial_chain_fp(start,
                                   (fin < start) ? capacity + fin - start :
                                                   fin - start,
                                   buckets_get_chns_fp(buckets)) &*&
    true == buckets_ok(buckets) &*&
    true == key_chains_start_on_hash_fp(buckets, 0, capacity) &*&
    key_opts == buckets_get_keys_fp(buckets) &*&
    length(buckets) == capacity;

  predicate item_list(list<void*> kaddrs, list<option<list<char> > > key_opts, list<int> values; list<map_item> items) =
    switch(kaddrs) {
      case nil:
        return key_opts == nil &*& values == nil &*& items == nil &*& distinct(items) == true &*& opt_no_dups(key_opts) == true;
      case cons(kaddrsh, kaddrst):
        return key_opts == cons(?key_optsh, ?key_optst) &*& values == cons(?valuesh, ?valuest) &*&
               item_list(kaddrst, key_optst, valuest, ?itemst) &*&
               (key_optsh == none ? (items == itemst) : (items == cons(map_item(kaddrsh, get_some(key_optsh), valuesh), itemst))) &*&
               map_no_dups(items) == opt_no_dups(key_opts); // TODO this might be better off outside of the predicate as a lemma...
    };

  // Keys, values, hashes -> key_opts, items
  predicate mapping_core(unsigned key_size, unsigned capacity,
                         void** kaddrs_ptr, int* busybits_ptr, unsigned* hashes_ptr, int* values_ptr; list<option<list<char> > > key_opts, list<map_item> items) =
     0 < capacity &*& 2*capacity < INT_MAX &*&
     pointers(kaddrs_ptr, capacity, ?kaddrs) &*&
     ints(busybits_ptr, capacity, ?busybits) &*&
     uints(hashes_ptr, capacity, ?hashes) &*&
     ints(values_ptr, capacity, ?values) &*&
     key_opt_list(key_size, kaddrs, busybits, key_opts) &*&
     length(key_opts) == capacity &*&
     hash_list(key_opts, hashes) == true &*&
     item_list(kaddrs, key_opts, values, items) &*&
     map_no_dups(items) == true;
  
  // Core + "chains" performance optimization
  predicate mapping(unsigned key_size, unsigned capacity,
                    void** kaddrs_ptr, int* busybits_ptr, unsigned* hashes_ptr, int* chains_ptr, int* values_ptr,
                    list<bucket<list<char> > > buckets;
                    list<option<list<char> > > key_opts,
                    list<map_item> items) =
     mapping_core(key_size, capacity, kaddrs_ptr, busybits_ptr, hashes_ptr, values_ptr, key_opts, items) &*&
     ints(chains_ptr, capacity, ?chains) &*&
     buckets_keys_insync(capacity, chains, buckets, key_opts);

  predicate mapp(struct os_map* map, unsigned key_size, unsigned capacity, list<map_item> items) =
    malloc_block_os_map(map) &*&
    map->kaddrs |-> ?kaddrs_ptr &*&
    map->busybits |-> ?busybits_ptr &*&
    map->hashes |-> ?hashes_ptr &*&
    map->chains |-> ?chains_ptr &*&
    map->values |-> ?values_ptr &*&
    map->capacity |-> capacity &*&
    map->key_size |-> key_size &*&
    malloc_block_pointers(kaddrs_ptr, capacity) &*&
    malloc_block_ints(busybits_ptr, capacity) &*&
    malloc_block_uints(hashes_ptr, capacity) &*&
    malloc_block_ints(chains_ptr, capacity) &*&
    malloc_block_ints(values_ptr, capacity) &*&
    mapping(key_size, capacity, kaddrs_ptr, busybits_ptr, hashes_ptr, chains_ptr, values_ptr, _, _, items) &*&
    is_pow2(capacity, N31) != none;
@*/

static uint64_t loop(unsigned k, uint64_t capacity)
//@ requires 0 < capacity &*& capacity < INT_MAX &*& is_pow2(capacity, N31) != none;
//@ ensures 0 <= result &*& result < capacity &*& result == loop_fp(k, capacity);
{
  //@ nat m = is_pow2_some(capacity, N31);
  //@ mod_bitand_equiv(k, capacity, m);
  //@ div_mod_gt_0(k % capacity, k, capacity);
  return k & (capacity - 1);
}

/*@
lemma list<char> extract_key_at_index(list<void*> kaddrs_b, list<int> busybits_b, list<option<list<char> > > key_opts_b, int n, list<int> busybits, list<option<list<char> > > key_opts)
  requires key_opt_list(?key_size, ?kaddrs, busybits, key_opts) &*&
           key_opt_list(key_size, kaddrs_b, busybits_b, key_opts_b) &*&
           0 <= n &*& n < length(busybits) &*& nth(n, busybits) != 0;
  ensures nth(n, key_opts) == some(result) &*& [0.25]chars(nth(n, kaddrs), key_size, result) &*&
          key_opt_list(key_size, drop(n+1, kaddrs), drop(n+1, busybits), drop(n+1, key_opts)) &*&
          key_opt_list(key_size,
                       append(reverse(take(n, kaddrs)), kaddrs_b),
                       append(reverse(take(n, busybits)), busybits_b),
                       append(reverse(take(n, key_opts)), key_opts_b));
{
  open key_opt_list(_, kaddrs, _, _);
  switch(busybits) {
    case nil:
      assert(length(busybits) == 0);
      return nil;
    case cons(bbh, bbt):
      switch(kaddrs) {
        case nil: return nil;
        case cons(kph, kpt):
          switch(key_opts) {
            case nil: return nil;
            case cons(kh, kt) :
            if (n == 0) {
              switch(kh) {
                case some(k):
                  return k;
                case none: return nil;
              }
            } else {
              close key_opt_list(key_size, cons(kph, kaddrs_b), cons(bbh, busybits_b), cons(kh, key_opts_b));
              append_reverse_take_cons(n,kph,kpt,kaddrs_b);
              append_reverse_take_cons(n,bbh,bbt,busybits_b);
              append_reverse_take_cons(n,kh,kt,key_opts_b);
              return extract_key_at_index(cons(kph,kaddrs_b),
                                          cons(bbh,busybits_b),
                                          cons(kh, key_opts_b),
                                          n-1, bbt, kt);
            }
          }
      }
  }
}

// ---

lemma void reconstruct_key_opt_list(list<void*> kaddrs1, list<int> busybits1, 
                                    list<void*> kaddrs2, list<int> busybits2)
  requires key_opt_list(?key_size, kaddrs1, busybits1, ?key_opts1) &*&
           key_opt_list(key_size, kaddrs2, busybits2, ?key_opts2);
  ensures key_opt_list(key_size,
                       append(reverse(kaddrs1), kaddrs2),
                       append(reverse(busybits1), busybits2),
                       append(reverse(key_opts1), key_opts2));
{
  open key_opt_list(key_size, kaddrs1, busybits1, key_opts1);
  switch(busybits1) {
    case nil:
      assert(kaddrs1 == nil);
      assert(key_opts1 == nil);
      return;
    case cons(bbh, bbt):
      append_reverse_tail_cons_head(kaddrs1, kaddrs2);
      append_reverse_tail_cons_head(busybits1, busybits2);
      append_reverse_tail_cons_head(key_opts1, key_opts2);
      close key_opt_list(key_size, cons(head(kaddrs1), kaddrs2), cons(bbh, busybits2), cons(head(key_opts1), key_opts2));
      reconstruct_key_opt_list(tail(kaddrs1), bbt, 
                               cons(head(kaddrs1), kaddrs2), cons(bbh, busybits2));
  }
}

lemma void recover_key_opt_list(list<void*> kaddrs, list<int> busybits, list<option<list<char> > > key_opts, int n)
  requires key_opt_list(?key_size, reverse(take(n, kaddrs)), reverse(take(n, busybits)), reverse(take(n, key_opts))) &*&
           nth(n, busybits) != 0 &*&
           [0.25]chars(nth(n, kaddrs), key_size, ?k) &*&
           nth(n, key_opts) == some(k) &*&
           key_opt_list(key_size, drop(n+1, kaddrs), drop(n+1, busybits), drop(n+1, key_opts)) &*&
           0 <= n &*& n < length(kaddrs) &*&
           n < length(busybits) &*&
           n < length(key_opts);
  ensures key_opt_list(key_size, kaddrs, busybits, key_opts);
{
  close key_opt_list(key_size,
                     cons(nth(n, kaddrs), drop(n+1,kaddrs)),
                     cons(nth(n, busybits), drop(n+1,busybits)),
                     cons(nth(n, key_opts), drop(n+1, key_opts)));
  drop_n_plus_one(n, kaddrs);
  drop_n_plus_one(n, busybits);
  drop_n_plus_one(n, key_opts);
  reconstruct_key_opt_list(reverse(take(n, kaddrs)),
                           reverse(take(n, busybits)),
                           drop(n, kaddrs),
                           drop(n, busybits));
}

// ---

lemma void key_opt_list_find_key(list<option<list<char> > > key_opts, int i, list<char> k)
  requires nth(i, key_opts) == some(k) &*&
           true == opt_no_dups(key_opts) &*&
           0 <= i &*& i < length(key_opts);
  ensures index_of(some(k), key_opts) == i;
{
  switch(key_opts) {
    case nil: return;
    case cons(h,t):
      if (h == some(k)) {
        no_dups_same(key_opts, k, i, 0);
        assert(i == 0);
        return;
      } else {
        key_opt_list_find_key(t, i-1, k);
      }
  }
}

// ---

lemma void no_hash_no_key(list<option<list<char> > > key_opts, list<unsigned> hashes, list<char> k, int i)
  requires true == hash_list(key_opts, hashes) &*&
           nth(i, hashes) != hash_fp(k) &*&
           0 <= i &*& i < length(key_opts);
  ensures nth(i, key_opts) != some(k);
{
  switch(key_opts) {
    case nil:
      assert(hashes == nil);
      return;
    case cons(kh,kt):
      assert hashes != nil;
      if (i == 0) {
        assert(nth(i, key_opts) == kh);
        if (kh == some(k)) {
          assert(head(hashes) == hash_fp(k));
          nth_0_head(hashes);
          assert(nth(i, hashes) == head(hashes));
          assert(nth(i, hashes) == hash_fp(k));
        }
        return;
      } else {
        nth_cons(i, tail(hashes), head(hashes));
        cons_head_tail(hashes);
        assert(nth(i, hashes) == nth(i-1,tail(hashes)));
        no_hash_no_key(kt, tail(hashes), k, i-1);
      }
  }
}

// ---

lemma void no_bb_no_key(list<option<list<char> > > key_opts, list<int> busybits, int i)
  requires key_opt_list(?key_size, ?kaddrs, busybits, key_opts) &*& 0 <= i &*& i < length(key_opts) &*&
           nth(i, busybits) == 0;
  ensures key_opt_list(key_size, kaddrs, busybits, key_opts) &*& nth(i, key_opts) == none;
{
  open key_opt_list(key_size, kaddrs, busybits, key_opts);
  switch(busybits) {
    case nil: ;
    case cons(bbh,bbt):
      if (i == 0) {
        nth_0_head(busybits);
        nth_0_head(key_opts);
      } else {
        no_bb_no_key(tail(key_opts), tail(busybits), i-1);
      }
  }
  close key_opt_list(key_size, kaddrs, busybits, key_opts);
}

// ---

lemma void up_to_neq_non_mem<t>(list<t> l, t x)
  requires true == up_to(nat_of_int(length(l)), (nthProp)(l, (neq)(x)));
  ensures false == mem(x, l);
{
  switch(l) {
    case nil:
    case cons(h,t):
      up_to_nth_uncons(h, t, nat_of_int(length(t)), (neq)(x));
      up_to_neq_non_mem(t, x);
  }
}

lemma void no_key_found<kt>(list<option<kt> > ks, kt k)
  requires true == up_to(nat_of_int(length(ks)), (nthProp)(ks, (neq)(some(k))));
  ensures false == mem(some(k), ks);
{
  up_to_neq_non_mem(ks, some(k));
}

// ---


lemma void hash_for_given_key(list<pair<list<char>, nat> > chains,
                              unsigned shift, unsigned capacity,
                              list<char> k)
  requires true == mem(k, map(fst, chains)) &*&
           true == forall(chains, (has_given_hash_fp)(shift, capacity));
  ensures true == (loop_fp(hash_fp(k), capacity) == shift);
{
  switch(chains) {
    case nil:
    case cons(h,t):
      if (fst(h) == k) {
      } else {
        hash_for_given_key(t, shift, capacity, k);
      }
  }
}

lemma void overshoot_bucket(list<bucket<list<char> > > buckets, unsigned shift, unsigned capacity, list<char> k)
  requires true == key_chains_start_on_hash_fp(buckets, shift, capacity) &*&
           loop_fp(hash_fp(k), capacity) < shift &*& shift <= capacity &*&
           capacity - shift == length(buckets);
  ensures false == exists(buckets, (bucket_has_key_fp)(k));
{
  switch(buckets) {
    case nil: return;
    case cons(bh,bt):
      switch(bh) { case bucket(chains):
        if (bucket_has_key_fp(k, bh)) {
            assert true == mem(k, map(fst, chains));
            assert true == forall(chains, (has_given_hash_fp)(shift, capacity));
            hash_for_given_key(chains, shift, capacity, k);
            assert true == (loop_fp(hash_fp(k), capacity) == shift);
        }
        overshoot_bucket(bt, shift + 1, capacity, k);
      }
  }
}

lemma void no_hash_not_in_this_bucket(list<pair<list<char>, nat> > chains, list<char> k,
                                      unsigned shift, unsigned capacity)
  requires true == forall(chains, (has_given_hash_fp)(shift, capacity)) &&
           shift != loop_fp(hash_fp(k), capacity);
  ensures false == mem(k, map(fst, chains));
{
  switch(chains) {
    case nil:
    case cons(h,t):
      if (fst(h) == k) {
        assert false;
      }
      no_hash_not_in_this_bucket(t, k, shift, capacity);
  }
}

lemma void wrong_hash_no_key(list<char> k, bucket<list<char> > bh, list<bucket<list<char> > > bt,
                             unsigned shift, unsigned capacity)
  requires true == key_chains_start_on_hash_fp(cons(bh,bt), shift, capacity) &*&
           shift != loop_fp(hash_fp(k), capacity);
  ensures false == bucket_has_key_fp(k, bh);
{
  switch(bh) { case bucket(chains):
    no_hash_not_in_this_bucket(chains, k, shift, capacity);
  }
}

lemma void key_is_contained_in_the_bucket_rec(list<bucket<list<char> > > buckets, list<pair<list<char>, nat> > acc,
                                              unsigned shift, unsigned capacity,
                                              list<char> k)
  requires true == key_chains_start_on_hash_fp(buckets, shift, capacity) &*&
           true == buckets_ok_rec(acc, buckets, capacity) &*&
           false == mem(k, map(fst, acc)) &*&
           0 <= shift &*& shift <= loop_fp(hash_fp(k), capacity) &*&
           0 < capacity &*&
           capacity - shift == length(buckets) &*&
           false == mem(k, map(fst, get_wraparound(acc, buckets))) &*&
           buckets != nil;
  ensures loop_fp(hash_fp(k), capacity) - shift < length(buckets) &*&
          mem(some(k), buckets_get_keys_rec_fp(acc, buckets)) ==
          bucket_has_key_fp(k, nth(loop_fp(hash_fp(k), capacity) - shift, buckets));
{
  switch(buckets) {
    case nil: return;
    case cons(bh,bt):
      loop_lims(hash_fp(k), capacity);
      assert true == ((loop_fp(hash_fp(k), capacity) - shift) <= length(buckets));
      if (loop_fp(hash_fp(k), capacity) == shift) {
        if (mem(some(k), buckets_get_keys_rec_fp(acc, buckets))) {
          some_bucket_contains_key_rec(acc, buckets, k);
          switch(bh) { case bucket(chains): }
          overshoot_bucket(bt, shift+1, capacity, k);
          if (bucket_has_key_fp(k, nth(loop_fp(hash_fp(k), capacity) - shift, buckets))) {
          } else {
            assert false;
          }
        } else {
          if (bucket_has_key_fp(k, nth(loop_fp(hash_fp(k), capacity) - shift, buckets))) {
            in_this_bucket_then_in_the_map(buckets,
                                           loop_fp(hash_fp(k), capacity) - shift,
                                           k, capacity, acc);
            assert false;
          } else {

          }
        }
      } else {
        assert true == (shift < loop_fp(hash_fp(k), capacity));
        assert true == (shift + 1 < capacity);
        assert true == (1 < length(buckets));
        assert true == (0 < length(bt));
        switch(bt) {
          case nil:
          case cons(h,t):
        }
        switch(bh) { case bucket(chains): };
        wrong_hash_no_key(k, bh, bt, shift, capacity);
        this_bucket_still_no_key(acc, bh, k);
        advance_acc_still_no_key(acc_at_this_bucket(acc, bh), k);
        key_is_contained_in_the_bucket_rec(bt, advance_acc(acc_at_this_bucket(acc, bh)),
                                           shift + 1, capacity, k);
        no_key_certainly_not_here(acc_at_this_bucket(acc, bh), k);
        assert some(k) != get_current_key_fp(acc_at_this_bucket(acc, bh));
        assert true == (bucket_has_key_fp(k, nth(loop_fp(hash_fp(k), capacity) - shift, buckets)) ==
                        bucket_has_key_fp(k, nth(loop_fp(hash_fp(k), capacity) - shift - 1, bt)));
      }
  }
}

lemma void bucket_has_key_correct_hash(list<bucket<list<char> > > buckets, list<char> k,
                                       unsigned start, unsigned capacity)
  requires true == exists(buckets, (bucket_has_key_fp)(k)) &*&
           true == key_chains_start_on_hash_fp(buckets, start, capacity) &*&
           start + length(buckets) == capacity;
  ensures true == bucket_has_key_fp(k, nth(loop_fp(hash_fp(k), capacity) - start, buckets));
{
  switch(buckets) {
    case nil:
    case cons(bh,bt):
      switch(bh) { case bucket(chains):
        if (bucket_has_key_fp(k, bh)) {
          if (start != loop_fp(hash_fp(k), capacity)) {
            no_hash_not_in_this_bucket(chains, k, start, capacity);
          }
        } else {
          bucket_has_key_correct_hash(bt, k, start + 1, capacity);
          if (loop_fp(hash_fp(k), capacity) < start + 1) {
            overshoot_bucket(bt, start + 1, capacity, k);
          }
        }
      }
  }
}

lemma void key_is_contained_in_the_bucket(list<bucket<list<char> > > buckets,
                                          unsigned capacity, list<char> k)
  requires true == key_chains_start_on_hash_fp(buckets, 0, capacity) &*&
           0 < capacity &*&
           true == buckets_ok(buckets) &*&
           length(buckets) == capacity;
  ensures mem(some(k), buckets_get_keys_fp(buckets)) == bucket_has_key_fp(k, nth(loop_fp(hash_fp(k), capacity), buckets));
{
  loop_lims(hash_fp(k), capacity);
  if (mem(k, map(fst, get_wraparound(nil, buckets)))) {
    key_in_wraparound_then_key_in_a_bucket(buckets, k, nil);
    bucket_has_key_correct_hash(buckets, k, 0, capacity);
    buckets_ok_wraparound_bounded_rec(get_wraparound(nil, buckets), buckets, capacity);
    buckets_ok_get_wraparound_idemp(buckets);
    key_in_wraparound_then_in_bucket(buckets, k);
  } else {
    buckets_ok_get_wraparound_idemp(buckets);
    key_is_contained_in_the_bucket_rec(buckets, get_wraparound(nil, buckets), 0, capacity, k);
  }
}

// ---

lemma void chains_depleted_no_hope(list<bucket<list<char> > > buckets, int i,
                                   int start, list<char> k, unsigned capacity)
  requires buckets != nil &*&
           true == up_to(nat_of_int(i + 1),
                         (byLoopNthProp)(buckets_get_keys_fp(buckets),
                                         (neq)(some(k)),
                                         capacity,
                                         start)) &*&
           true == key_chains_start_on_hash_fp(buckets, 0, capacity) &*&
           true == buckets_ok(buckets) &*&
           0 <= i &*& i < capacity &*&
           0 <= start &*&
           start < capacity &*&
           capacity == length(buckets) &*&
           nth(loop_fp(start + i, capacity), buckets_get_chns_fp(buckets)) == 0;
  ensures false == bucket_has_key_fp(k, nth(start, buckets));
{
  if (bucket_has_key_fp(k, nth(start, buckets))) {
    crossing_chains_keep_key(buckets, i, start, capacity, k);
    assert true == mem(k, map(fst, get_crossing_chains_fp(buckets, loop_fp(start + i, capacity))));
    loop_lims(start + i, capacity);
    no_crossing_chains_here(buckets, loop_fp(start + i, capacity));
    assert get_crossing_chains_fp(buckets, loop_fp(start + i, capacity)) == nil;
  }
}
@*/

static int find_key(void** kaddrs, int* busybits, unsigned* hashes, int* chains, void* key, unsigned key_hash, uint64_t key_size, uint64_t capacity)
/*@ requires mapping(key_size, capacity, kaddrs, busybits, hashes, chains, ?values, ?buckets, ?key_opts, ?items) &*&
             [?kfr]chars(key, key_size, ?k) &*&
             hash_fp(k) == key_hash &*&
             is_pow2(capacity, N31) != none; @*/
/*@ ensures mapping(key_size, capacity, kaddrs, busybits, hashes, chains, values, buckets, key_opts, items) &*&
            [kfr]chars(key, key_size, k) &*&
            mem(some(k), key_opts) ? result == index_of(some(k), key_opts) :
                                     result == -1; @*/
{
  //@ open mapping(key_size, capacity, kaddrs, busybits, hashes, chains, values, buckets, key_opts, items);
  //@ open mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, key_opts, items);
  //@ assert pointers(kaddrs, capacity, ?kaddrs_lst);
  //@ assert ints(busybits, capacity, ?busybits_lst);
  //@ assert uints(hashes, capacity, ?hashes_lst);
  //@ assert ints(chains, capacity, ?chains_lst);
  //@ assert ints(values, capacity, ?values_lst);
  //@ open buckets_keys_insync(capacity, chains_lst, buckets, key_opts);
  //@ assert key_opt_list(key_size, kaddrs_lst, busybits_lst, key_opts);
  uint64_t start = loop(key_hash, capacity);
  unsigned i = 0;
  for (; i < capacity; ++i)
    /*@ invariant key_opt_list(key_size, kaddrs_lst, busybits_lst, key_opts) &*&
                  length(busybits_lst) == length(key_opts) &*&
                  pointers(kaddrs, capacity, kaddrs_lst) &*&
                  ints(busybits, capacity, busybits_lst) &*&
                  uints(hashes, capacity, hashes_lst) &*&
                  ints(chains, capacity, chains_lst) &*&
                  ints(values, capacity, values_lst) &*&
                  item_list(kaddrs_lst, key_opts, values_lst, items) &*&
                  0 <= i &*& i <= capacity &*&
                  [kfr]chars(key, key_size, k) &*&
                  hash_fp(k) == key_hash &*&
                  true == hash_list(key_opts, hashes_lst) &*&
                  start == loop_fp(hash_fp(k), capacity) &*&
                  key_opts == buckets_get_keys_fp(buckets) &*&
                  buckets != nil &*&
                  true == up_to(nat_of_int(i), (byLoopNthProp)(key_opts, (neq)(some(k)), capacity, start));
    @*/
    //@ decreases capacity - i;
  {
    uint64_t index = loop(start + i, capacity);
    void* kp = kaddrs[index];
    int bb = busybits[index];
    unsigned kh = hashes[index];
    int chn = chains[index];
    if (bb != 0 && kh == key_hash) {
      //@ close key_opt_list(key_size, nil, nil, nil);
      //@ extract_key_at_index(nil, nil, nil, index, busybits_lst, key_opts);
      //@ append_nil(reverse(take(index, kaddrs_lst)));
      //@ append_nil(reverse(take(index, busybits_lst)));
      //@ append_nil(reverse(take(index, key_opts)));
      if (generic_eq(kp, key, key_size)) {
        //@ recover_key_opt_list(kaddrs_lst, busybits_lst, key_opts, index);
        //@ open item_list(kaddrs_lst, key_opts, values_lst, items);
        //@ assert true == distinct(items);
        //@ assert distinct(items) == opt_no_dups(key_opts);
        //@ assert true == opt_no_dups(key_opts);
        //@ close item_list(kaddrs_lst, key_opts, values_lst, items);
        //@ key_opt_list_find_key(key_opts, index, k);
        //@ close buckets_keys_insync(capacity, chains_lst, buckets, key_opts);
        //@ close mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, key_opts, items);
        //@ close mapping(key_size, capacity, kaddrs, busybits, hashes, chains, values, buckets, key_opts, items);
        return (int)index;
      }
      //@ recover_key_opt_list(kaddrs_lst, busybits_lst, key_opts, index);
    } else {
      //@ if (bb != 0) no_hash_no_key(key_opts, hashes_lst, k, index);
      //@ if (bb == 0) no_bb_no_key(key_opts, busybits_lst, index);
      if (chn == 0) {
        //@ assert length(chains_lst) == capacity;
        //@ buckets_keys_chns_same_len(buckets);
        //@ assert length(buckets) == capacity;
        //@ no_crossing_chains_here(buckets, index);
        //@ assert nil == get_crossing_chains_fp(buckets, index);
        //@ key_is_contained_in_the_bucket(buckets, capacity, k);
        //@ assert true == up_to(nat_of_int(i), (byLoopNthProp)(key_opts, (neq)(some(k)), capacity, start));
        //@ assert true == up_to(nat_of_int(i), (byLoopNthProp)(key_opts, (neq)(some(k)), capacity, loop_fp(hash_fp(k), capacity)));
        //@ assert true == up_to(succ(nat_of_int(i)), (byLoopNthProp)(key_opts, (neq)(some(k)), capacity, loop_fp(hash_fp(k), capacity)));
        //@ assert true == up_to(nat_of_int(i+1), (byLoopNthProp)(key_opts, (neq)(some(k)), capacity, loop_fp(hash_fp(k), capacity)));
        //@ assert buckets != nil;
        //@ chains_depleted_no_hope(buckets, i, loop_fp(hash_fp(k), capacity), k, capacity);
        //@ assert false == mem(some(k), key_opts);
        //@ close buckets_keys_insync(capacity, chains_lst, buckets, key_opts);
        //@ close mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, key_opts, items);
        //@ close mapping(key_size, capacity, kaddrs, busybits, hashes, chains, values, buckets, key_opts, items);
        return -1;
      }
      //@ assert(length(key_opts) == capacity);
    }
    //@ assert(nth(index, key_opts) != some(k));
    //@ assert(true == neq(some(k), nth(index, key_opts)));
    //@ assert(true == neq(some(k), nth(loop_fp(i+start,capacity), key_opts)));
    //@ assert(nat_of_int(i+1) == succ(nat_of_int(i)));
  }
  //@ by_loop_for_all(key_opts, (neq)(some(k)), start, capacity, nat_of_int(capacity));
  //@ no_key_found(key_opts, k);
  //@ close buckets_keys_insync(capacity, chains_lst, buckets, key_opts);
  //@ close mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, key_opts, items);
  //@ close mapping(key_size, capacity, kaddrs, busybits, hashes, chains, values, buckets, key_opts, items);
  return -1;
}

/*@
fixpoint int opts_size<kt>(list<option<kt> > opts) {
  switch(opts) {
    case nil: return 0;
    case cons(h,t): return (h == none ? 0 : 1) + opts_size(t);
  }
}

// ---

fixpoint bool cell_busy(option<list<char> > x) { return x != none; }

lemma void full_size(list<option<list<char> > > key_opts)
  requires true == up_to(nat_of_int(length(key_opts)), (nthProp)(key_opts, cell_busy));
  ensures opts_size(key_opts) == length(key_opts);
{
  switch(key_opts) {
    case nil: return;
    case cons(h,t):
      up_to_nth_uncons(h, t, nat_of_int(length(t)), cell_busy);
      full_size(t);
  }
}

// ---
  
lemma void key_opts_size_limits(list<option<list<char> > > key_opts)
  requires true;
  ensures 0 <= opts_size(key_opts) &*& opts_size(key_opts) <= length(key_opts);
{
  switch(key_opts) {
    case nil: return;
    case cons(h,t):
      key_opts_size_limits(t);
  }
}

lemma void zero_bbs_is_for_empty(list<int> busybits, list<option<list<char> > > key_opts, int i)
  requires key_opt_list(?key_size, ?kaddrs, busybits,  key_opts) &*&
           nth(i, busybits) == 0 &*&
           0 <= i &*& i < length(busybits);
  ensures key_opt_list(key_size, kaddrs, busybits, key_opts) &*&
          nth(i, key_opts) == none &*&
          opts_size(key_opts) < length(key_opts);
{
  open key_opt_list(key_size, kaddrs, busybits, key_opts);
  switch(busybits) {
    case nil: break;
    case cons(h,t):
      if (i == 0) {
        assert head(key_opts) == none;
        key_opts_size_limits(tail(key_opts));
      } else {
        nth_cons(i, t, h);
        zero_bbs_is_for_empty(t, tail(key_opts), i-1);
      }
  }
  close key_opt_list(key_size, kaddrs, busybits, key_opts);
}

// ---

lemma void start_Xchain(unsigned capacity, list<int> chains,  list<bucket<list<char> > > buckets, list<option<list<char> > > key_opts, int start)
  requires buckets_keys_insync(capacity, chains, buckets, key_opts) &*&
           0 <= start &*& start < capacity;
  ensures buckets_keys_insync_Xchain(capacity, chains, buckets, start, start, key_opts);
{
  buckets_keys_chns_same_len(buckets);
  open buckets_keys_insync(capacity, chains, buckets, key_opts);
  add_part_chn_zero_len(buckets_get_chns_fp(buckets), start);
  close buckets_keys_insync_Xchain(capacity, chains, buckets, start, start, key_opts);
}

// ---
  
lemma void bb_nonzero_cell_busy(list<int> busybits, list<option<list<char> > > key_opts, int i)
  requires key_opt_list(?key_size, ?kaddrs, busybits, key_opts) &*& 
           nth(i, busybits) != 0 &*&
           0 <= i &*& i < length(busybits);
  ensures key_opt_list(key_size, kaddrs, busybits, key_opts) &*& 
          true == cell_busy(nth(i, key_opts));
  {
    open key_opt_list(key_size, kaddrs, busybits, key_opts);
    switch(busybits) {
      case nil: break;
      case cons(h,t):
      if (i == 0) {
      } else {
        nth_cons(i, t, h);
        bb_nonzero_cell_busy(t, tail(key_opts), i-1);
      }
    }
    close key_opt_list(key_size, kaddrs, busybits, key_opts);
  }
@*/


static unsigned find_empty(int* busybits, int* chains, uint64_t start, uint64_t capacity)
/*@ requires mapping_core(?key_size, capacity, ?kaddrs, busybits, ?hashes, ?values, ?key_opts, ?items) &*&
             ints(chains, capacity, ?old_chains_lst) &*&
             buckets_keys_insync(capacity, old_chains_lst, ?buckets, key_opts) &*&
             0 <= start &*& start < capacity &*&
             opts_size(key_opts) < capacity &*&
             is_pow2(capacity, N31) != none; @*/
/*@ ensures mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, key_opts, items) &*&
            ints(chains, capacity, ?new_chains_lst) &*&
            buckets_keys_insync_Xchain(capacity, new_chains_lst, buckets, start, result, key_opts) &*&
            nth(result, key_opts) == none &*&
            0 <= result &*& result < capacity; @*/
{
  //@ open mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, key_opts, items);
  //@ start_Xchain(capacity, old_chains_lst, buckets, key_opts, start);
  //@ loop_bijection(start, capacity);
  unsigned i = 0;
  for (; i < capacity; ++i)
    /*@ invariant key_opt_list(key_size, ?kaddrs_lst, ?busybits_lst, key_opts) &*&
                  ints(busybits, capacity, busybits_lst) &*&
                  uints(hashes, capacity, ?hashes_lst) &*&
                  pointers(kaddrs, capacity, kaddrs_lst) &*&
                  ints(values, capacity, ?values_lst) &*&
                  ints(chains, capacity, ?invariant_chains_lst) &*&
                  length(key_opts) == capacity &*&
                  true == hash_list(key_opts, hashes_lst) &*&
                  item_list(kaddrs_lst, key_opts, values_lst, items) &*&
                  true == map_no_dups(items) &*&
                  0 <= i &*& i <= capacity &*&
                  true == up_to(nat_of_int(i),(byLoopNthProp)(key_opts, cell_busy, capacity, start)) &*&
                  buckets_keys_insync_Xchain(capacity, invariant_chains_lst, buckets, start, loop_fp(start + i, capacity), key_opts);
      @*/
    //@ decreases capacity - i;
  {
    uint64_t index = loop(start + i, capacity);
    //@ assert ints(chains, capacity, ?chains_lst);
    //@ open buckets_keys_insync_Xchain(capacity, chains_lst, buckets, start, index, key_opts);
    int bb = busybits[index];
    if (0 == bb) {
      //@ zero_bbs_is_for_empty(busybits_lst, key_opts, index);
      //@ close mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, key_opts, items);
      //@ close buckets_keys_insync_Xchain(capacity, chains_lst, buckets, start, index, key_opts);
      return index;
    }
    int chn = chains[index];
    //@ buckets_keys_chns_same_len(buckets);
    //@ buckets_ok_chn_bound(buckets, index);
    //@ outside_part_chn_no_effect(buckets_get_chns_fp(buckets), start, index, capacity);
    //@ assert chn <= capacity;
    //@ assert capacity < INT_MAX;
    chains[index] = chn + 1;
    //@ bb_nonzero_cell_busy(busybits_lst, key_opts, index);
    //@ assert true == cell_busy(nth(loop_fp(i+start,capacity), key_opts));
    //@ assert nat_of_int(i+1) == succ(nat_of_int(i));
    //@ Xchain_add_one(chains_lst, buckets_get_chns_fp(buckets), start, index < start ? capacity + index - start : index - start, capacity);
    /*@
        if (i + 1 == capacity) {
          by_loop_for_all(key_opts, cell_busy, start, capacity, nat_of_int(capacity));
          full_size(key_opts);
          assert false;
        }
    @*/
    /*@
        if (index < start) {
          if (start + i < capacity) loop_bijection(start + i, capacity);
          loop_injection_n(start + i + 1 - capacity, capacity, 1);
          loop_bijection(start + i + 1 - capacity, capacity);
          loop_injection_n(start + i - capacity, capacity, 1);
          loop_bijection(start + i - capacity, capacity);
        } else {
          if (capacity <= start + i) {
            loop_injection_n(start + i - capacity, capacity, 1);
            loop_bijection(start + i - capacity, capacity);
          }
          loop_bijection(start + i, capacity);
          if (start + i + 1 == capacity) {
            loop_injection_n(start + i + 1 - capacity, capacity, 1);
            loop_bijection(start + i + 1 - capacity, capacity);
          } else {
            loop_bijection(start + i + 1, capacity);
          }
        }
      @*/
    //@ close buckets_keys_insync_Xchain(capacity, chains_lst, buckets, start, index, key_opts);
  }
  // pred_mapping_same_len(bbs, ks);
  //@ by_loop_for_all(key_opts, cell_busy, start, capacity, nat_of_int(capacity));
  //@ full_size(key_opts);
  //@ close mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, key_opts, items);
  return -1;
}

/*@
fixpoint list<int> zero_list_fp(nat len) {
  return repeat_n<int>(len, 0);
}

// ---

lemma void move_int(int* data, int i, int len)
  requires ints(data, i, ?l1) &*& ints(data + i, len - i, ?l2) &*&
           i < len;
  ensures ints(data, i + 1, append(l1,cons(head(l2),nil))) &*&
          ints(data + i + 1, len - i - 1, tail(l2));
{
  open(ints(data, i, l1));
  switch(l1) {
    case nil:
      open(ints(data, len-i, l2));
      close(ints(data, 1, cons(head(l2),nil)));
    case cons(h,t):
      move_int(data+1, i-1, len-1);
  }
  close(ints(data, i+1, append(l1, cons(head(l2),nil))));
}

// ---

lemma void extend_zero_list(nat len, int extra)
  requires true;
  ensures update(int_of_nat(len), 0, append(zero_list_fp(len), cons(extra,nil))) == zero_list_fp(succ(len));
{
  switch(len) {
    case zero: return;
    case succ(l):
      extend_zero_list(l, extra);
  }
}

// ---

fixpoint list<option<list<char> > > none_list_fp(nat len) {
  return repeat_n<option<list<char> > >(len, none);
}

// ---

lemma void nat_len_of_non_nil<t>(t h, list<t> t)
  requires true;
  ensures nat_of_int(length(cons(h, t)) - 1) == nat_of_int(length(t)) &*&
          nat_of_int(length(cons(h, t))) == succ(nat_of_int(length(t)));
{
  int l = length(cons(h,t));
  assert(0 < l);
  switch(nat_of_int(l)) {
    case zero:
      note(int_of_nat(zero) == l);
      assert(false);
      return;
    case succ(lll):
      return;
  }
}

lemma void produce_key_opt_list(unsigned key_size, list<unsigned> hashes, list<void*> kaddrs)
  requires length(hashes) == length(kaddrs);
  ensures key_opt_list(key_size, kaddrs, zero_list_fp(nat_of_int(length(kaddrs))), none_list_fp(nat_of_int(length(kaddrs)))) &*&
          length(kaddrs) == length(none_list_fp(nat_of_int(length(kaddrs))));
{
  switch(kaddrs) {
    case nil:
      close key_opt_list(key_size, kaddrs, zero_list_fp(nat_of_int(length(kaddrs))), none_list_fp(nat_of_int(length(kaddrs))));
      return;
    case cons(kaddrh,kaddrt):
      switch(hashes) {
        case nil: break;
        case cons(hh,ht): break;
      }
      assert(hashes != nil);
      produce_key_opt_list(key_size, tail(hashes), kaddrt);
      nat_len_of_non_nil(kaddrh,kaddrt);
      close key_opt_list(key_size, kaddrs, zero_list_fp(nat_of_int(length(kaddrs))), none_list_fp(nat_of_int(length(kaddrs))));
      return;
  }
}

// ---

lemma void confirm_hash_list_for_nones(list<unsigned> hashes)
  requires true;
  ensures true == hash_list(none_list_fp(nat_of_int(length(hashes))), hashes);
{
  switch(hashes) {
    case nil:
      return;
    case cons(h,t):
      confirm_hash_list_for_nones(t);
      nat_len_of_non_nil(h,t);
      assert(tail(none_list_fp(nat_of_int(length(hashes)))) == none_list_fp(nat_of_int(length(t))));
      return;
  }
}

// ---

lemma void nat_gt_zero_not_zero(int n)
  requires n > 0;
  ensures nat_of_int(n) != zero;
{
  assert int_of_nat(nat_of_int(n)) == n;
  assert int_of_nat(nat_of_int(n)) != 0;
  assert nat_of_int(n) != zero;
}

lemma void empty_keychains_start_on_hash(nat len, int pos, unsigned capacity)
  requires 0 < capacity;
  ensures true == key_chains_start_on_hash_fp(empty_buckets_fp<list<char> >(len), pos, capacity);
{
  switch(len) {
    case zero:
    case succ(n):
      empty_keychains_start_on_hash(n, pos + 1, capacity);
  }
}

lemma void empty_buckets_insync(list<int> chains, unsigned capacity)
  requires chains == zero_list_fp(nat_of_int(capacity)) &*&
           0 < capacity;
  ensures buckets_keys_insync(capacity, chains,
                              empty_buckets_fp<list<char> >(nat_of_int(capacity)),
                              none_list_fp(nat_of_int(capacity)));
{
  empty_buckets_chns_zeros<list<char> >(nat_of_int(capacity));
  nat_gt_zero_not_zero(capacity);
  assert nat_of_int(capacity) != zero;
  empty_buckets_ok<list<char> >(nat_of_int(capacity));
  empty_buckets_ks_none<list<char> >(nat_of_int(capacity));
  empty_keychains_start_on_hash(nat_of_int(capacity), 0, capacity);
  repeat_n_length(nat_of_int(capacity), bucket(nil));
  assert length(empty_buckets_fp<list<char> >(nat_of_int(capacity))) == int_of_nat(nat_of_int(capacity));
  assert length(empty_buckets_fp<list<char> >(nat_of_int(capacity))) == capacity;
  close buckets_keys_insync(capacity, chains, empty_buckets_fp<list<char> >(nat_of_int(capacity)), none_list_fp(nat_of_int(capacity)));
}

// ---

lemma void repeat_none_is_opt_no_dups<t>(nat n, list<option<t> > opts)
  requires opts == repeat_n(n, none);
  ensures true == opt_no_dups(opts);
{
  switch(n) {
    case zero:
      assert opts == nil;
    case succ(p):
      assert opts == cons(?optsh, ?optst);
      repeat_none_is_opt_no_dups(p, optst);
      assert optsh == none;
  }
}

lemma void produce_empty_item_list(unsigned capacity, list<void*> kaddrs, list<int> values)
  requires length(kaddrs) == length(values) &*& length(kaddrs) == capacity;
  ensures item_list(kaddrs, none_list_fp(nat_of_int(capacity)), values, nil);
{
  switch(kaddrs) {
    case nil:
      length_0_nil(values);
      close item_list(kaddrs, none_list_fp(nat_of_int(capacity)), values, nil);
    case cons(kh,kt):
      assert values == cons(?vh,?vt);
      assert capacity > 0;
      repeat_n_length(nat_of_int(capacity), none);
      assert none_list_fp(nat_of_int(capacity)) == cons(?noh, ?not);
      repeat_n_is_n(nat_of_int(capacity), none);
      assert noh == none;
      assert head(none_list_fp(nat_of_int(capacity))) == none;
      produce_empty_item_list(capacity - 1, kt, vt);
      repeat_n_tail(nat_of_int(capacity), none);
      assert not == none_list_fp(nat_of_int(capacity-1));
      assert true == distinct(nil);
      repeat_none_is_opt_no_dups(nat_of_int(capacity), none_list_fp(nat_of_int(capacity)));
      assert true == opt_no_dups(none_list_fp(nat_of_int(capacity)));
      close item_list(kaddrs, none_list_fp(nat_of_int(capacity)), values, nil);
  }
}
@*/

struct os_map* os_map_init(uint64_t key_size, uint64_t capacity)
/*@ requires 0 < capacity &*& capacity <= MAP_CAPACITY_MAX &*&
             0 < key_size; @*/
/*@ ensures result == 0 ? true : mapp(result, key_size, capacity, nil); @*/
{

  #ifndef OS_MAP_CAPACITY_NOTPOW2
  // Check that capacity is a power of 2
  if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
      return (struct os_map*) 0;
  }
  //@ check_pow2_valid(capacity);
  #endif

  struct os_map* map = (struct os_map*) malloc(sizeof(struct os_map));
  void** kaddrs = (void**) malloc(capacity * sizeof(void*));
  int* busybits = (int*) malloc(capacity * sizeof(int));
  unsigned* hashes = (unsigned*) malloc(capacity * sizeof(unsigned));
  int* chains = (int*) malloc(capacity * sizeof(int));
  uint64_t* values = (uint64_t*) malloc(capacity * sizeof(uint64_t));

  if(map == 0 || kaddrs == 0 || busybits == 0 || hashes == 0 || chains == 0 || values == 0) {
    if(map != 0) {
      free(map);
    }
    if(kaddrs != 0) {
      free(kaddrs);
    }
    if(busybits != 0) {
      free(busybits);
    }
    if(hashes != 0) {
      free(hashes);
    }
    if(chains != 0) {
      free(chains);
    }
    if(values != 0) {
      free(values);
    }
    return 0;
  }

  //@ assert pointers(kaddrs, capacity, ?kaddrs_lst);
  //@ assert ints(busybits, capacity, ?busybits_lst);
  //@ assert uints(hashes, capacity, ?hashes_lst);
  //@ assert ints(chains, capacity, ?chains_lst);
  //@ assert ints(values, capacity, ?values_lst);
  unsigned i = 0;
  for (; i < capacity; ++i)
    /*@ invariant ints(busybits, i, zero_list_fp(nat_of_int(i))) &*&
                  ints(busybits + i, capacity - i, drop(i, busybits_lst)) &*&
                  ints(chains, i, zero_list_fp(nat_of_int(i))) &*&
                  ints(chains + i, capacity - i, drop(i, chains_lst)) &*&
                  0 < capacity &*& 2*capacity < INT_MAX &*&
                  0 <= i &*& i <= capacity;
      @*/
    //@ decreases capacity - i;
  {
    //@ move_int(busybits, i, capacity);
    //@ move_int(chains, i, capacity);
    //@ extend_zero_list(nat_of_int(i), head(drop(i, busybits_lst)));
    //@ extend_zero_list(nat_of_int(i), head(drop(i, chains_lst)));
    busybits[i] = 0;
    chains[i] = 0;
    //@ assert(succ(nat_of_int(i)) == nat_of_int(i+1));
    //@ tail_drop(busybits_lst, i);
    //@ tail_drop(chains_lst, i);
  }
  //@ assert(drop(i,busybits_lst) == nil);
  //@ open ints(busybits + i, capacity - i, drop(i,busybits_lst));
  //@ produce_key_opt_list(key_size, hashes_lst, kaddrs_lst);
  //@ assert ints(chains, capacity, ?zeroed_chains_lst);
  //@ empty_buckets_insync(zeroed_chains_lst, capacity);
  //@ produce_empty_item_list(capacity, kaddrs_lst, values_lst);
  //@ confirm_hash_list_for_nones(hashes_lst);
  //@ close mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, _, _);
  //@ close mapping(key_size, capacity, kaddrs, busybits, hashes, chains, values, _, _, nil);

  map->kaddrs = kaddrs;
  map->busybits = busybits;
  map->hashes = hashes;
  map->chains = chains;
  map->values = values;
  map->capacity = capacity;
  map->key_size = key_size;

  //@ close mapp(map, key_size, capacity, nil);
  return map;
}


/*@
lemma void map_remove_unrelated_key(list<map_item> items, map_item it1, map_item it2)
  requires mapp_item(remove(it1, items), ?k2, some(it2)) &*&
           true == map_no_dups(items) &*&
           it1 == map_item(_, ?k1, _) &*&
           it1 != it2 &*&
           k2 != k1;
  ensures mapp_item(items, k2, some(it2));
{
  switch(items) {
    case nil:
      assert remove(it1, items) == items;
    case cons(h,t):
      if (h == it1) {
        assert remove(it1, items) == t;
        assert mapp_item(t, k2, some(it2));
        close mapp_item(cons(it1, t), k2, some(it2));
      } else {
        mapp_item_key_match(remove(it1, items), it2);
        assert it2 == map_item(_, k2, _);
        open mapp_item(remove(it1, items), k2, some(it2));
        if (h == it2) {
          mapp_item_from_head(it2, t);
          mapp_item_key_match(cons(it2, t), it2);
          assert mapp_item(cons(it2, t), k2, some(it2));
        } else {
          map_remove_unrelated_key(t, it1, it2);
          assert map_item_key(h) != k2;
          close mapp_item(items, k2, some(it2));
        }
      }
  }
}

lemma void mapp_item_is_in_items(list<map_item> items, list<char> k, map_item it)
  requires mapp_item(items, k, some(it));
  ensures mapp_item(items, k, some(it)) &*&
          true == mem(it, items);
{
  switch(items) {
    case nil:
      open mapp_item(items, k, some(it));
      assert false;
    case cons(h,t):
      if (h == it) {
        assert true == mem(it, cons(h,t));
      } else {
        open mapp_item(items, k, some(it));
        mapp_item_is_in_items(t, k, it);
        close mapp_item(items, k, some(it));
      }
  }
}

lemma void produce_mapp_item_when_key_opts_has(list<option<list<char> > > key_opts, list<char> k, list<map_item> items)
  requires item_list(?kaddrs, key_opts, ?values, items) &*&
           true == mem(some(k), key_opts) &*&
           true == map_no_dups(items);
  ensures item_list(kaddrs, key_opts, values, items) &*&
          mapp_item(items, k, ?it) &*&
          it != none;
{
  switch(key_opts) {
    case nil:
      assert false;
    case cons(h,t):
      open item_list(kaddrs, key_opts, values, items);
      if (h == some(k)) {
        map_item current = map_item(head(kaddrs), k, head(values));
        assert head(items) == current;
        close mapp_item(items, k, some(current));
      } else if (h == none) {
        produce_mapp_item_when_key_opts_has(t, k, items);
      } else {
        assert h == some(?notk);
        map_item current = map_item(head(kaddrs), notk, head(values));
        produce_mapp_item_when_key_opts_has(t, k, remove(current, items));
        assert mapp_item(remove(current, items), k, ?maybeit);
        switch(maybeit) {
          case none:
            assert some(current) != maybeit;
          case some(it):
            mapp_item_is_in_items(remove(current, items), k, it);
            assert maybeit != some(current);
            mapp_item_key_match(remove(current, items), it);
            assert it == map_item(_, k, _);
            assert k != notk;
            map_remove_unrelated_key(items, current, it);
        }
      }
      close item_list(kaddrs, key_opts, values, items);
  }
}

lemma void mapp_item_not_in_extended_items(list<map_item> items, map_item it1)
  requires mapp_item(items, ?k2, ?maybeit2) &*&
           true == map_no_dups(items) &*&
           it1 == map_item(_, ?k1, _) &*&
           some(it1) != maybeit2 &*&
           k1 != k2;
  ensures mapp_item(cons(it1, items), k2, maybeit2);
{
  switch(items) {
    case nil:
      open mapp_item(nil, k2, maybeit2);
      assert maybeit2 == none;
      close mapp_item(cons(it1, items), k2, none);
    case cons(h,t):
      close mapp_item(cons(it1, items), k2, maybeit2);
  }
}

lemma void produce_mapp_item_when_key_opts_has_not(list<option<list<char> > > key_opts, list<char> k, list<map_item> items)
  requires item_list(?kaddrs, key_opts, ?values, items) &*&
           false == mem(some(k), key_opts) &*&
           true == map_no_dups(items);
  ensures item_list(kaddrs, key_opts, values, items) &*&
          mapp_item(items, k, none);
{
  switch(key_opts) {
    case nil:
      open item_list(kaddrs, key_opts, values, items);
      assert length(key_opts) == 0;
      assert length(items) == 0;
      length_0_nil(items);
      assert items == nil;
      close mapp_item(items, k, none);
      close item_list(kaddrs, key_opts, values, items);
    case cons(maybeh,t):
      open item_list(kaddrs, key_opts, values, items);
      switch(maybeh) {
        case none:
          produce_mapp_item_when_key_opts_has_not(t, k, items);
        case some(h):
          produce_mapp_item_when_key_opts_has_not(t, k, tail(items));
          assert h != k;
          mapp_item_not_in_extended_items(tail(items), head(items));
      }
      close item_list(kaddrs, key_opts, values, items);
  }
}

lemma void produce_mapp_item(list<option<list<char> > > key_opts, list<char> k, list<map_item> items)
  requires item_list(?kaddrs, key_opts, ?values, items) &*&
           true == map_no_dups(items);
  ensures item_list(kaddrs, key_opts, values, items) &*&
          mapp_item(items, k, ?it) &*&
          mem(some(k), key_opts) == (it != none);
{
  if (mem(some(k), key_opts)) {
    produce_mapp_item_when_key_opts_has(key_opts, k, items);
  } else {
    produce_mapp_item_when_key_opts_has_not(key_opts, k, items);
  }
}

// ---


lemma void opt_no_dups_means_index_of_plus_one<t>(option<t> head, list<option<t> > tail, t value)
  requires true == opt_no_dups(cons(head, tail)) &*&
           true == mem(some(value), tail) &*&
           some(value) != head;
  ensures index_of(some(value), tail) + 1 == index_of(some(value), cons(head, tail));
{
  switch(tail) {
    case nil:
      assert false;
    case cons(th, tt):
      if (th == some(value)) {
        assert index_of(some(value), tail) == 0;
        assert index_of(some(value), cons(head, tail)) == 1;
      } else {
      }
  }
}

lemma void map_item_key_in_items_key_opts(map_item it, list<map_item> items)
  requires item_list(?kaddrs, ?key_opts, ?values, items) &*&
           true == map_no_dups(items) &*&
           true == mem(it, items) &*&
           it == map_item(?ka, ?k, ?v);
  ensures item_list(kaddrs, key_opts, values, items) &*&
          true == mem(some(k), key_opts);
{
  switch(kaddrs) {
    case nil:
      open item_list(kaddrs, key_opts, values, items);
      assert false;
    case cons(kaddrsh, kaddrst):
      open item_list(kaddrs, key_opts, values, items);
      assert key_opts == cons(?key_optsh, ?key_optst);
      switch(key_optsh) {
        case none:
          map_item_key_in_items_key_opts(it, items);
        case some(kh):
          if (kh == k) {
            assert true == mem(some(k), key_opts);
          } else {
            assert items == cons(?itemsh, ?itemst);
            map_item_key_in_items_key_opts(it, itemst);
          }
      }
      close item_list(kaddrs, key_opts, values, items);
  }
}

lemma void map_item_key_not_in_items_key_opts(map_item it, list<map_item> items)
  requires item_list(?kaddrs, ?key_opts, ?values, items) &*&
           it == map_item(_, ?k, _) &*&
           false == mem(some(k), key_opts);
  ensures item_list(kaddrs, key_opts, values, items) &*&
          false == mem(it, items);
{
  switch(kaddrs) {
    case nil:
      open item_list(kaddrs, key_opts, values, items);
      assert items == nil;
      close item_list(kaddrs, key_opts, values, items);
    case cons(kaddrsh, kaddrst):
      open item_list(kaddrs, key_opts, values, items);
      assert key_opts == cons(?key_optsh, ?key_optst);
      switch(key_optsh) {
        case none:
          map_item_key_not_in_items_key_opts(it, items);
        case some(kh):
          if (kh == k) {
            assert false;
          } else {
            assert items == cons(?itemsh, ?itemst);
            map_item_key_not_in_items_key_opts(it, itemst);
          }
      }
      close item_list(kaddrs, key_opts, values, items);
  }
}

lemma void item_list_same_index(list<void*> kaddrs, list<option<list<char> > > key_opts, list<int> values, map_item it)
  requires item_list(kaddrs, key_opts, values, ?items) &*&
           true == map_no_dups(items) &*&
           true == mem(it, items) &*&
           it == map_item(?ka, ?k, ?v);
  ensures item_list(kaddrs, key_opts, values, items) &*&
          it == map_item(nth(index_of(some(k), key_opts), kaddrs), get_some(nth(index_of(some(k), key_opts), key_opts)), nth(index_of(some(k), key_opts), values));
{
  // this is horrible; clearly the key_optsh == none and key_optsh != some(k) cases are the same and should be refactored...
  switch(kaddrs) {
    case nil:
      open item_list(kaddrs, key_opts, values, items);
      assert items != nil;
      assert false;
    case cons(kaddrsh, kaddrst):
      open item_list(kaddrs, key_opts, values, items);
      assert key_opts == cons(?key_optsh, ?key_optst);
      assert values == cons(?valuesh, ?valuest);
      switch(key_optsh) {
        case none:
          item_list_same_index(kaddrst, key_optst, valuest, it);
          assert it == map_item(nth(index_of(some(k), key_optst), kaddrst), get_some(nth(index_of(some(k), key_optst), key_optst)), nth(index_of(some(k), key_optst), valuest));

          map_item_key_in_items_key_opts(it, items);
          assert true == mem(some(k), key_opts);
          assert key_optsh != some(k);
          assert true == mem(some(k), key_optst);

          opt_no_dups_means_index_of_plus_one(key_optsh, key_optst, k);
          assert index_of(some(k), key_optst) == index_of(some(k), key_opts) - 1;

          map_item_key_in_items_key_opts(it, items);
          assert true == mem(some(k), key_opts);
          assert key_optsh != some(k);
          assert true == mem(some(k), key_optst);

          mem_index_of(some(k), key_optst);
          assert index_of(some(k), key_optst) >= 0;

          nth_cons(index_of(some(k), key_opts), kaddrst, kaddrsh);
          assert nth(index_of(some(k), key_optst), kaddrst) == nth(index_of(some(k), key_opts), kaddrs);
          close item_list(kaddrs, key_opts, values, items);
        case some(kh):
          if (kh == k) {
            assert items == cons(?itemsh, ?itemst);
            assert itemsh == map_item(kaddrsh, get_some(key_optsh), valuesh);
            close item_list(kaddrs, key_opts, values, items);
            map_item_key_in_items_key_opts(it, items);
            assert index_of(some(k), key_opts) == 0;
            assert false == mem(some(k), key_optst);
            open item_list(kaddrs, key_opts, values, items);
            map_item_key_not_in_items_key_opts(it, itemst);
            assert it == itemsh;
            close item_list(kaddrs, key_opts, values, items);
          } else {
            assert items == cons(?itemsh, ?itemst);
            assert itemsh == map_item(_, ?kitemsh, _);
            assert kitemsh == get_some(key_optsh);
            assert kh != k;
            assert kitemsh != k;
            assert it != itemsh;
            item_list_same_index(kaddrst, key_optst, valuest, it);
            assert it == map_item(nth(index_of(some(k), key_optst), kaddrst), get_some(nth(index_of(some(k), key_optst), key_optst)), nth(index_of(some(k), key_optst), valuest));
            map_item_key_in_items_key_opts(it, itemst);
            assert true == mem(some(k), key_opts);
            assert key_optsh != some(k);
            assert true == mem(some(k), key_optst);
            opt_no_dups_means_index_of_plus_one(key_optsh, key_optst, k);
            assert index_of(some(k), key_optst) == index_of(some(k), key_opts) - 1;

            close item_list(kaddrs, key_opts, values, items);
            map_item_key_in_items_key_opts(it, items);
            assert true == mem(some(k), key_opts);
            assert key_optsh != some(k);
            assert true == mem(some(k), key_optst);

            mem_index_of(some(k), key_optst);
            assert index_of(some(k), key_optst) >= 0;

            nth_cons(index_of(some(k), key_opts), kaddrst, kaddrsh);
            assert nth(index_of(some(k), key_optst), kaddrst) == nth(index_of(some(k), key_opts), kaddrs);
          }
      }
  }
}

lemma void mapp_item_has_correct_values(list<map_item> items, list<char> k)
  requires mapp_item(items, k, some(?it)) &*&
           item_list(?kaddrs, ?key_opts, ?values, items) &*&
           true == map_no_dups(items);
  ensures mapp_item(items, k, some(it)) &*&
          item_list(kaddrs, key_opts, values, items) &*&
          it == map_item(nth(index_of(some(k), key_opts), kaddrs), k, nth(index_of(some(k), key_opts), values));
{
  mapp_item_key_match(items, it);
  assert it == map_item(_, k, _);

  mapp_item_is_in_items(items, k, it);
  assert true == mem(it, items);

  item_list_same_index(kaddrs, key_opts, values, it);
  assert k == get_some(nth(index_of(some(k), key_opts), key_opts));
}
@*/

bool os_map_get(struct os_map* map, void* key_ptr, uint64_t* value_out)
/*@ requires mapp(map, ?key_size, ?capacity, ?items) &*&
             chars(key_ptr, key_size, ?key) &*&
             *value_out |-> ?old_value; @*/
/*@ ensures mapp(map, key_size, capacity, items) &*&
            chars(key_ptr, key_size, key) &*&
            switch(map_item_keyed(key, items)) { 
              case none: return result == false &*& *value_out |-> old_value; 
              case some(it): return result == true &*& it == map_item(_, key, ?value) &*& *value_out |-> value;
            }; @*/
{
  //@ open mapp(map, key_size, capacity, items);
  unsigned hash = generic_hash(key_ptr, map->key_size);
  int index = find_key(map->kaddrs, map->busybits, map->hashes, map->chains, key_ptr, hash, map->key_size, map->capacity);
  //@ open mapping(key_size, capacity, map->kaddrs, map->busybits, map->hashes, map->chains, map->values, ?buckets, ?key_opts, items);
  //@ open mapping_core(key_size, capacity, map->kaddrs, map->busybits, map->hashes, map->values, key_opts, items);
  //@ produce_mapp_item(key_opts, key, items);
  if (-1 == index)
  {
    //@ close mapping_core(key_size, capacity, map->kaddrs, map->busybits, map->hashes, map->values, key_opts, items);
    //@ close mapping(key_size, capacity, map->kaddrs, map->busybits, map->hashes, map->chains, map->values, buckets, key_opts, items);
    //@ close mapp(map, key_size, capacity, items);
    return false;
  }
  *value_out = map->values[index];
  //@ mapp_item_has_correct_values(items, key);
  //@ close mapping_core(key_size, capacity, map->kaddrs, map->busybits, map->hashes, map->values, key_opts, items);
  //@ close mapping(key_size, capacity, map->kaddrs, map->busybits, map->hashes, map->chains, map->values, buckets, key_opts, items);
  //@ close mapp(map, key_size, capacity, items);
  return true;
}

/*@
lemma void put_keeps_key_opt_list(list<void*> kaddrs, list<int> busybits, list<option<list<char> > > key_opts, int index, void* key, list<char> k)
  requires key_opt_list(?key_size, kaddrs, busybits, key_opts) &*&
           [0.25]chars(key, key_size, k) &*&
           0 <= index &*& index < length(busybits) &*&
           nth(index, key_opts) == none;
  ensures key_opt_list(key_size, update(index, key, kaddrs), update(index, 1, busybits), update(index, some(k), key_opts));
{
  open key_opt_list(key_size, kaddrs, busybits, key_opts);
  switch(busybits) {
    case nil:
      break;
    case cons(bbh, bbt):
      if (index == 0) {
        tail_of_update_0(kaddrs, key);
        tail_of_update_0(key_opts, some(k));
        head_update_0(key, kaddrs);
      } else {
        put_keeps_key_opt_list(tail(kaddrs), bbt, tail(key_opts), index-1, key, k);
        cons_head_tail(kaddrs);
        cons_head_tail(key_opts);
        update_tail_tail_update(head(kaddrs), tail(kaddrs), index, key);
        update_tail_tail_update(head(key_opts), tail(key_opts), index, some(k));
        update_tail_tail_update(bbh, bbt, index, 1);
      }
      update_non_nil(kaddrs, index, key);
      update_non_nil(key_opts, index, some(k));
  }
  close key_opt_list(key_size, update(index, key, kaddrs), update(index, 1, busybits), update(index, some(k), key_opts));
}

// ---

lemma void items_length_is_opts_size(list<map_item> items, list<option<list<char> > > key_opts)
  requires item_list(?kaddrs, key_opts, ?values, items);
  ensures item_list(kaddrs, key_opts, values, items) &*&
          length(items) == opts_size(key_opts);
{
  open item_list(kaddrs, key_opts, values, items);
  switch(kaddrs) {
    case nil:
      break;
    case cons(kaddrsh, kaddrst):
      assert key_opts == cons(?key_optsh, ?key_optst);
      switch(key_optsh) {
        case none:
          items_length_is_opts_size(items, key_optst);
          assert opts_size(key_opts) == opts_size(key_optst);
        case some(kh):
          assert items == cons(?itemsh, ?itemst);
          items_length_is_opts_size(itemst, key_optst);
          assert length(items) == length(itemst) + 1;
          assert opts_size(key_opts) == opts_size(key_optst) + 1;
      }
  }
  close item_list(kaddrs, key_opts, values, items);
}

// ---

lemma void put_preserves_no_dups(list<option<list<char> > > key_opts, int i, list<char> k)
  requires false == mem(some(k), key_opts) &*& 
           true == opt_no_dups(key_opts);
  ensures true == opt_no_dups(update(i, some(k), key_opts));
{
  switch(key_opts) {
    case nil: break;
    case cons(h,t):
      if (i == 0) {
      } else {
        put_preserves_no_dups(t, i-1, k);
        if (h == none) {
        } else {
          assert(false == mem(h, t));
          update_irrelevant_cell(h, i-1, some(k), t);
          assert(false == mem(h, update(i-1, some(k), t)));
        }
      }
  }
}

// ---

lemma void put_preserves_hash_list(list<option<list<char> > > key_opts, list<unsigned> hashes, int index, list<char> k, unsigned hash)
  requires true == hash_list(key_opts, hashes) &*&
           hash_fp(k) == hash &*&
           0 <= index;
  ensures true == hash_list(update(index, some(k), key_opts), update(index, hash, hashes));
{
  switch(key_opts) {
    case nil: break;
    case cons(h,t):
      update_non_nil(hashes, index, hash);
      if (index == 0) {
        head_update_0(some(k), key_opts);
        head_update_0(hash, hashes);
        tail_of_update_0(hashes, hash);
        assert update(0, hash, hashes) != nil;
        assert true == hash_list(t, tail(update(0, hash, hashes)));
        assert head(update(0, hash, hashes)) == hash_fp(get_some(head(update(0, some(k), key_opts))));
      } else {
        put_preserves_hash_list(t, tail(hashes), index-1, k, hash);
        cons_head_tail(hashes);
        update_tail_tail_update(head(hashes), tail(hashes), index, hash);
        update_tail_tail_update(h, t, index, some(k));
      }
  }
}

// ---

lemma void mapp_item_none_key_is_absent(list<option<list<char> > > key_opts, list<map_item> items, list<char> k)
  requires item_list(?kaddrs, key_opts, ?values, items) &*&
           mapp_item(items, k, none);
  ensures item_list(kaddrs, key_opts, values, items) &*&
          mapp_item(items, k, none) &*&
          false == mem(some(k), key_opts);
{
  open item_list(kaddrs, key_opts, values, items);
  switch(kaddrs) {
    case nil:
      assert key_opts == nil;
    case cons(kaddrsh, kaddrst):
      assert key_opts == cons(?key_optsh, ?key_optst);
      switch (key_optsh) {
        case none:
          mapp_item_none_key_is_absent(key_optst, items, k);
        case some(kh):
          assert items == cons(?itemsh, ?itemst);
          assert itemsh == map_item(?ihka, ?ihk, ?ihv);
          open mapp_item(items, k, none);
          assert ihk != k;
          mapp_item_none_key_is_absent(key_optst, itemst, k);
          close mapp_item(items, k, none);
      }
  }
  close item_list(kaddrs, key_opts, values, items);
}

// ---

lemma void buckets_put_chains_still_start_on_hash(list<bucket<list<char> > > buckets, list<char> k, int shift,
                                                  int start, int dist, unsigned capacity)
  requires true == key_chains_start_on_hash_fp(buckets, shift, capacity) &*&
           loop_fp(hash_fp(k), capacity) == start + shift &*&
           0 <= start &*& start < length(buckets) &*&
           0 <= dist &*& dist < capacity;
  ensures true == key_chains_start_on_hash_fp(buckets_put_key_fp(buckets, k, start, dist), shift, capacity);
{
  switch(buckets) {
    case nil:
    case cons(h,t):
      switch(h) {
        case bucket(chains):
          if (start == 0) {
            assert true == has_given_hash_fp(shift, capacity, pair(k, nat_of_int(dist)));
          } else {
            buckets_put_chains_still_start_on_hash(t, k, shift + 1, start - 1, dist, capacity);
          }
      }
  }
}

lemma void buckets_keys_put_key_insync(unsigned capacity, list<int> chains, int start,
                                       int fin, list<char> k, list<option<list<char> > > key_opts)
  requires buckets_keys_insync_Xchain(capacity, chains, ?buckets, start, fin, key_opts) &*&
           0 <= start &*& start < capacity &*&
           0 <= fin &*& fin < capacity &*&
           false == mem(k, buckets_all_keys_fp(buckets)) &*&
           start == loop_fp(hash_fp(k), capacity) &*&
           nth(fin, buckets_get_keys_fp(buckets)) == none;
  ensures buckets_keys_insync(capacity, chains,
                              buckets_put_key_fp(buckets, k, start,
                                                 loop_fp(capacity + fin - start,
                                                         capacity)),
                              update(fin, some(k), key_opts));
{
  open buckets_keys_insync_Xchain(capacity, chains, buckets, start, fin, key_opts);
  int dist = 0;
  if (fin == 0 && start != 0) {
    dist = capacity - start;
    loop_bijection(capacity + fin - start, capacity);
    loop_injection(0, capacity);
    loop_bijection(0, capacity);
  } else if (fin < start) {
    dist = fin + capacity - start;
    loop_bijection(fin - start + capacity, capacity);
    loop_injection_n(fin + capacity, capacity, -1);
  } else {
    dist = fin - start;
    loop_injection_n(fin - start + capacity, capacity, -1);
    loop_bijection(fin - start, capacity);
  }
  buckets_add_part_get_chns(buckets, k, start, dist);
  assert loop_fp(capacity + fin - start, capacity) == dist;
  loop_bijection(fin, capacity);
  assert loop_fp(start + dist, capacity) == fin;
  buckets_put_still_ok(buckets, k, start, dist);
  buckets_put_chains_still_start_on_hash(buckets, k, 0, start, dist, length(buckets));
  buckets_put_update_ks(buckets, key_opts, k, start, dist);
  assert length(buckets) == capacity;
  buckets_put_key_length_unchanged(buckets, k, start, dist);
  assert length(buckets_put_key_fp(buckets, k, start, dist)) == capacity;
  close buckets_keys_insync(capacity, chains,
                            buckets_put_key_fp(buckets, k, start, dist),
                            update(fin, some(k), key_opts));
}

// ---

lemma void put_updates_item_list(list<void*> kaddrs, list<option<list<char> > > key_opts, list<int> values,
                                 int index, void* ka, list<char> k, int v)
  requires item_list(kaddrs, key_opts, values, ?items) &*&
           0 <= index &*& index < length(key_opts) &*&
           nth(index, key_opts) == none &*&
           true == map_no_dups(items) &*&
           false == mem(some(k), key_opts);
  ensures item_list(update(index, ka, kaddrs), update(index, some(k), key_opts), update(index, v, values), ?new_items) &*&
          length(new_items) == length(items) + 1 &*&
          true == subset(items, new_items) &*&
          true == contains(new_items, map_item(ka, k, v));
{
  assume(false); // TODO this is definitely doable
  /
  switch(kaddrs) {
    case nil:
      open item_list(kaddrs, key_opts, values, items);
      assert false;
    case cons(kaddrsh, kaddrst):
      open item_list(kaddrs, key_opts, values, items);
      assert key_opts == cons(?key_optsh, ?key_optst);
      assume(true == opt_no_dups(update(index, some(k), key_opts))); // TODO
      assume(false == mem(k, map(map_item_key, items))); // TODO
      assume(true == map_no_dups(cons(map_item(ka, k, v), items))); // TODO
      if (index == 0) {
        assert key_optsh == none;
        close item_list(update(index, ka, kaddrs), update(index, some(k), key_opts), update(index, v, values), cons(map_item(ka, k, v), items));
        subset_cons(map_item(ka, k, v), items);
      } else {
        assert values == cons(?valuesh, ?valuest);
        put_updates_item_list(kaddrst, key_optst, valuest, index - 1, ka, k, v);
        //update_tail_tail_update(key_optsh, key_optst, index, some(k));
        assert true == opt_no_dups(cons(key_optsh, update(index - 1, some(k), key_optst)));
        open item_list(update(index - 1, ka, kaddrst),
                       update(index - 1, some(k), key_optst),
                       update(index - 1, v, valuest),
                       ?sub_items);
        assert true == map_no_dups(sub_items);
        close item_list(update(index - 1, ka, kaddrst),
                        update(index - 1, some(k), key_optst),
                        update(index - 1, v, valuest),
                        sub_items);
      }
  }
  /
}

// ---

lemma void items_contains_item_key(list<map_item> items, map_item it)
  requires true == mem(it, items) &*&
           it == map_item(_, ?k, _);
  ensures true == mem(k, map(map_item_key, items));
{
  switch(items) {
    case nil:
      assert false;
    case cons(h, t):
      if (h == it) {
        assert k == map_item_key(h);
      } else {
        items_contains_item_key(t, it);
      }
  }
}

lemma void items_contain_mapp_item(list<map_item> items, map_item it)
  requires true == mem(it, items) &*&
           true == map_no_dups(items) &*&
           it == map_item(_, ?k, _);
  ensures mapp_item(items, k, some(it));
{
  switch(items) {
    case nil:
      assert false;
    case cons(h,t):
      if(h == it) {
        close mapp_item(items, k, some(h));
      } else {
        items_contain_mapp_item(t, it);
        items_contains_item_key(t, it);
        close mapp_item(cons(h,t), k, some(it));
      }
  }
}

// ---

lemma void destroy_empty_mapp_item()
  requires mapp_item(?items, ?k, none);
  ensures true;
{
  open mapp_item(items, k, none);
  switch(items) {
    case nil:
      break;
    case cons(h, t):
      destroy_empty_mapp_item();
  }
}
@*/

void os_map_put(struct os_map* map, void* key_ptr, uint64_t value)
/*@ requires mapp(map, ?key_size, ?capacity, ?items) &*&
             [0.25]chars(key_ptr, key_size, ?key) &*&
             length(items) < capacity &*&
             map_item_keyed(key, items) == none; @*/
/*@ ensures mapp(map, key_size, capacity, ?new_items) &*&
            length(new_items) == length(items) + 1 &*&
            true == subset(items, new_items) &*&
            map_item_keyed(key, new_items) == some(map_item(key_ptr, key, value)); @*/
{
  //@ open mapp(map, key_size, capacity, items);
  unsigned hash = generic_hash(key_ptr, map->key_size);
  //@ open mapping(key_size, capacity, ?kaddrs, ?busybits, ?hashes, ?chains, ?values, ?buckets, ?key_opts, items);
  //@ assert map->kaddrs == kaddrs && map->busybits == busybits && map->hashes == hashes && map->chains == chains && map->values == values;
  //@ open mapping_core(key_size, capacity, kaddrs, busybits, map->hashes, values, key_opts, items);
  //@ assert map->capacity == capacity && capacity < INT_MAX;
  //@ close mapping_core(key_size, capacity, kaddrs, busybits, map->hashes, values, key_opts, items);
  uint64_t start = loop(hash, map->capacity);
  //@ open mapping_core(key_size, capacity, kaddrs, busybits, map->hashes, values, key_opts, items);
  //@ items_length_is_opts_size(items, key_opts);
  //@ close mapping_core(key_size, capacity, kaddrs, busybits, map->hashes, values, key_opts, items);
  int index = find_empty(map->busybits, map->chains, start, map->capacity);
  //@ open mapping_core(key_size, capacity, kaddrs, busybits, map->hashes, values, key_opts, items);
  //@ assert pointers(kaddrs, capacity, ?kaddrs_lst);
  //@ assert ints(busybits, capacity, ?busybits_lst);
  //@ put_keeps_key_opt_list(kaddrs_lst, busybits_lst, key_opts, index, key, k);
  //@ assert ints(values, capacity, ?values_lst);
  //@ mapp_item_none_key_is_absent(key_opts, items, k);
  //@ open item_list(kaddrs_lst, key_opts, values_lst, items);
  //@ assert true == opt_no_dups(key_opts);
  //@ close item_list(kaddrs_lst, key_opts, values_lst, items);
  //@ assert uints(hashes, capacity, ?hashes_lst);
  map->kaddrs[index] = key_ptr;
  map->busybits[index] = 1;
  map->hashes[index] = hash;
  map->values[index] = value;
  //@ open buckets_keys_insync_Xchain(capacity, ?cur_chains, buckets, ?cur_start, ?cur_fin, key_opts);
  //@ assert buckets_get_keys_fp(buckets) == key_opts;
  //@ close buckets_keys_insync_Xchain(capacity, cur_chains, buckets, cur_start, cur_fin, key_opts);
  //@ no_key_in_ks_no_key_in_buckets(buckets, k);
  //@ assert ints(chains, capacity, ?chains_lst);
  //@ buckets_keys_put_key_insync(capacity, chains_lst, start, index, k, key_opts);
  //@ put_updates_item_list(kaddrs_lst, key_opts, values_lst, index, key, k, value);
  //@ put_preserves_no_dups(key_opts, index, k);
  //@ put_preserves_hash_list(key_opts, hashes_lst, index, k, hash);
  //@ list<option<list<char> > > new_key_opts = update(index, some(k), key_opts);
  //@ open item_list(update(index, key, kaddrs_lst), new_key_opts, update(index, value, values_lst), ?new_items);
  //@ assert true == map_no_dups(new_items);
  //@ close item_list(update(index, key, kaddrs_lst), new_key_opts, update(index, value, values_lst), new_items);
  //@ destroy_empty_mapp_item();
  //@ items_contain_mapp_item(new_items, map_item(key, k, value));
  //@ close mapping_core(key_size, capacity, kaddrs, busybits, hashes, values, new_key_opts, new_items);
  //@ close mapping(key_size, capacity, kaddrs, busybits, hashes, chains, values, _, new_key_opts, new_items);
  //@ close mapp(map, key_size, capacity, new_items);
}

/*
  lemma void map_erase_decrement_len<kt, vt>(list<pair<kt, vt> > m, kt k)
  requires true == map_has_fp(m, k);
  ensures length(m) == 1 + length(map_erase_fp(m, k));
  {
    switch(m) {
      case nil:
      case cons(h,t):
        switch(h) { case pair(key,val):
          if (key != k) map_erase_decrement_len(t, k);
        }
    }
  }

  lemma void map_get_mem<kt,vt>(list<pair<kt,vt> > m, kt k)
  requires true == map_has_fp(m, k);
  ensures true == mem(pair(k, map_get_fp(m, k)), m);
  {
    switch(m) {
      case nil:
      case cons(h,t):
        switch(h) { case pair(key, val):
          if (key != k) map_get_mem(t, k);
        }
    }
  }
  @*/

void os_map_erase(struct os_map* map, void* key_ptr)
/*@ requires mapp(map, ?key_size, ?capacity, ?items) &*&
             [?frac]chars(key_ptr, key_size, ?key) &*&
             map_item_keyed(key, items) == some(map_item(key_ptr, key, _)); @*/
/*@ ensures mapp(map, key_size, capacity, ?new_items) &*&
            length(new_items) == length(items) - 1 &*&
            true == subset(new_items, items) &*&
            map_item_keyed(key, new_items) == none &*&
            [frac + 0.25]chars(key_ptr, key_size, key); @*/
{
  //@ open mapp<t>(map, kp, hsh, recp, mapc(capacity, contents, addrs));
  unsigned hash = generic_hash(key_ptr, map->key_size);
  map_impl_erase(map->busybits,
                 map->kaddrs,
                 map->hashes,
                 map->chains,
                 key_ptr,
                 hash,
                 map->key_size,
                 map->capacity);
  // map_erase_decrement_len(contents, k);
  //--map->size;
  /*@
    close mapp<t>(map, kp, hsh, recp, mapc(capacity,
                                           map_erase_fp(contents, k),
                                           map_erase_fp(addrs, k)));
    @*/
}

/*
  lemma void map_has_two_values_nondistinct<kt,vt>(list<pair<kt,vt> > m, kt k1, kt k2)
  requires true == map_has_fp(m, k1) &*&
          true == map_has_fp(m, k2) &*&
          map_get_fp(m, k1) == map_get_fp(m, k2) &*&
          k1 != k2;
  ensures false == distinct(map(snd, m));
  {
    switch(m) {
      case nil:
      case cons(h,t):
        switch(h) { case pair(key, value):
          if (key == k1) {
            map_get_mem(t, k2);
            mem_map(pair(k2, map_get_fp(m, k1)), t, snd);
          } else if (key == k2) {
            map_get_mem(t, k1);
            mem_map(pair(k1, map_get_fp(m, k2)), t, snd);
          } else {
            map_has_two_values_nondistinct(t, k1, k2);
          }
        }
    }
  }

  lemma void map_erase_keep_inv<kt,vt>(list<pair<kt, vt> > m,
                                       kt key,
                                       fixpoint (pair<kt, vt>, bool) inv)
  requires true == forall(m, inv);
  ensures true == forall(map_erase_fp(m, key), inv);
  {
    switch(m) {
      case nil:
      case cons(h,t):
        switch(h) {case pair(k, v): }
        map_erase_keep_inv(t, key, inv);
    }
  }

  lemma void map_erase_all_keep_inv<kt,vt>(list<pair<kt, vt> > m,
                                           list<kt> keys,
                                           fixpoint (pair<kt, vt>, bool) inv)
  requires true == forall(m, inv);
  ensures true == forall(map_erase_all_fp(m, keys), inv);
  {
    switch(keys) {
      case nil:
      case cons(h,t):
        map_erase_all_keep_inv(m, t, inv);
        map_erase_keep_inv(map_erase_all_fp(m, t), h, inv);
    }
  }
  @*/
  
/*
lemma void produce_empty_key_vals(list<void*> kaddrs, list<int> val_arr)
  requires length(kaddrs) == length(val_arr);
  ensures key_vals(kaddrs, none_list_fp(nat_of_int(length(val_arr))), val_arr, nil);
{
  switch(val_arr) {
    case nil:
      length_0_nil(kaddrs);
      assert kaddrs == nil;
      close key_vals(kaddrs, none_list_fp(nat_of_int(length(val_arr))), val_arr, nil);
      return;
    case cons(vh,vt):
      assert kaddrs == cons(?kh,?kt);
      produce_empty_key_vals(kt,vt);
      nat_len_of_non_nil(vh,vt);
      close key_vals(kaddrs, none_list_fp(nat_of_int(length(val_arr))), val_arr, nil);
      return;
  }
}

lemma void kopts_has_not_then_keys_has_not(list<option<list<char> > > ks, list<map_item> items, list<char> key);
  requires key_vals(?addr_arr, ks, ?val_arr, items) &*&
           false == mem(some(key), ks) &*& 
           mapp_item(items, key, ?it);
  ensures key_vals(addr_arr, ks, val_arr, items) &*& 
          mapp_item(items, key, it) &*& 
          it == none;
{
  switch(ks) {
    case nil:
      assert key_vals(addr_arr, ks, val_arr, items);
      return;
    case cons(h,t):
      open key_vals(addr_arr, ks, val_arr, items);
      if (h == some(key)) {
      } else {
        if (h == none) {
          kopts_has_not_then_keys_has_not(t, items, key);
        } else {
          kopts_has_not_then_keys_has_not(t, remove(map_item(get_some(h), head(addr_arr), head(val_arr)), items), key);
          map_remove_unrelated_key(items, key, map_item(get_some(h), head(addr_arr), head(val_arr)));
        }
      }
      close key_vals(addr_arr, ks, val_arr, items);
      return;
  }
}

lemma void hmapping_ks_capacity(hmap hm, unsigned capacity)
  requires hm == hmap(?key_opts, _) &*&
           hmapping(?key_size, capacity, ?kaddrs, ?busybits, ?hashes, hm);
  ensures hmapping(key_size, capacity, kaddrs, busybits, hashes, hm) &*&
          length(key_opts) == capacity;
{
  open hmapping(key_size, capacity, kaddrs, busybits, hashes, hm);
  assert key_opt_list(key_size, kaddrs, busybits, key_opts);
  key_opt_list_same_len(busybits, key_opts);
  close hmapping(key_size, capacity, kaddrs, busybits, hashes, hm);
}

// somehow VeriFast can't expand map_no_dups by itself... this nudges it
lemma void remove_preserves_map_no_dups(list<map_item> items)
  requires true == map_no_dups(items) &*&
           items == cons(?h, ?t);
  ensures true == map_no_dups(t);
{
  assert true == map_no_dups(items);
  switch(h) {
    case map_item(ka, k, v):
      assert false == mem(ka, map(map_item_addr, t)) && false == mem(h, t) && true == map_no_dups(t);
      assert true == map_no_dups(t);
  }
}
*/

/*
lemma void map_has_this_key<kt,vt>(list<pair<kt,vt> > m, pair<kt,vt> kv)
requires true == mem(kv, m);
ensures true == map_has_fp(m, fst(kv));
  {
    switch(m) {
      case nil: return;
      case cons(h,t):
        if (h == kv) {
        } else {
          map_has_this_key(t, kv);
        }
    }
  }

  lemma void map_no_dups_returns_the_key<kt,vt>(list<pair<kt, vt> > m,
                                                pair<kt, vt> kv)
  requires true == mem(kv, m) &*& true == no_dup_keys(m);
  ensures map_get_fp(m, fst(kv)) == snd(kv);
  {
    switch(m) {
      case nil: return;
      case cons(h,t):
        if (h == kv) {
        } else {
          map_has_this_key(t, kv);
          assert(true == map_has_fp(t, fst(kv)));
          if (fst(h) == fst(kv)) {
          }
          assert(fst(h) != fst(kv));
          map_no_dups_returns_the_key(t, kv);
        }
    }
  }

  lemma void ks_find_returns_the_key<kt,vt>(list<option<kt> > ks,
                                            list<vt> val_arr,
                                            list<pair<kt, vt> > m, kt k)
  requires key_vals(ks, val_arr, m) &*& true == opt_no_dups(ks) &*&
           true == mem(some(k), ks);
  ensures key_vals(ks, val_arr, m) &*&
          nth(index_of(some(k), ks), val_arr) == map_get_fp(m, k);
  {
    switch(ks) {
      case nil:
        open key_vals(ks, val_arr, m);
        close key_vals(ks, val_arr, m);
      case cons(h,t):
        map_no_dups(ks, m);
        open key_vals(ks, val_arr, m);
        if (h == some(k)) {
          nth_0_head(val_arr);
          assert(index_of(some(k), ks) == 0);
          assert(nth(0, val_arr) == head(val_arr));
          assert(nth(index_of(some(k), ks), val_arr) == head(val_arr));
          assert(true == mem(pair(k,head(val_arr)), m));
          map_no_dups_returns_the_key(m, pair(k, head(val_arr)));
          assert(map_get_fp(m, k) == head(val_arr));
        } else if (h == none) {
          ks_find_returns_the_key(t, tail(val_arr), m, k);
          assert(val_arr != nil);
          mem_index_of(some(k), t);
          nth_cons(index_of(some(k), t) + 1, tail(val_arr), head(val_arr));
          cons_head_tail(val_arr);
        } else {
          ks_find_returns_the_key(t, tail(val_arr),
                                  remove(pair(get_some(h),
                                              head(val_arr)),
                                         m),
                                  k);
          map_remove_unrelated_key(m, k, pair(get_some(h), head(val_arr)));
          assert(index_of(some(k), ks) == 1 + index_of(some(k), t));

          assert(val_arr != nil);
          mem_index_of(some(k), t);
          nth_cons(index_of(some(k), t) + 1, tail(val_arr), head(val_arr));
          cons_head_tail(val_arr);
        }
        close key_vals(ks, val_arr, m);
    }
  }
*/


// ========================================================================================
// BELOW THIS IS MAP-IMPL !!!
// ====================================================


//@ #include <list.gh>
//@ #include <listex.gh>
//@ #include <nat.gh>
//@ #include "proof/stdex.gh"
//@ #include "proof/map.gh"
//@ #include "proof/natlist.gh"

/*@
  predicate hmapping<kt>(predicate (void*; kt) keyp,
                         fixpoint (kt, unsigned) hash,
                         unsigned capacity,
                         int* busybits,
                         list<void*> kps,
                         unsigned* k_hashes;
                         hmap<kt> m);



  fixpoint int hmap_size_fp<kt>(hmap<kt> m) {
    return ks_size_fp(hmap_ks_fp(m));
  }

  fixpoint bool hmap_empty_cell_fp<kt>(hmap<kt> m, int index) {
    return (nth(index, hmap_ks_fp(m)) == none);
  }

fixpoint bool hmap_exists_key_fp<kt>(hmap<kt> m, kt k) {
  return mem(some(k), hmap_ks_fp(m));
}

  fixpoint int hmap_find_key_fp<kt>(hmap<kt> m, kt k) {
    return index_of(some(k), hmap_ks_fp(m));
  }

  fixpoint hmap<kt> hmap_put_key_fp<kt>(hmap<kt> m, int i, kt k, unsigned hash) {
    switch(m) { case hmap(ks, khs):
      return hmap(update(i, some(k), ks), update(i, hash, khs));
    }
  }

  fixpoint hmap<kt> hmap_rem_key_fp<kt>(hmap<kt> m, int i) {
    switch(m) { case hmap(ks, khs):
      return hmap(update(i, none, ks), khs);
    }
  }

  lemma void hmap_rem_preserves_no_dups<kt>(list<option<kt> > ks, int i)
  requires true == opt_no_dups(ks) &*& 0 <= i;
  ensures true == opt_no_dups(update(i, none, ks));
  {
    rem_preserves_opt_no_dups(ks, i);
  }

  lemma void hmap_rem_preserves_hash_list<kt>(list<option<kt> > vals,
                                              list<unsigned> hashes,
                                              fixpoint (kt, unsigned) hash,
                                              int i)
  requires true == hash_list(vals, hashes, hash) &*& 0 <= i;
  ensures true == hash_list(update(i, none, vals), hashes, hash);
  {
    switch(vals){
      case nil: break;
      case cons(h,t):
        if (i == 0) {
        } else {
          hmap_rem_preserves_hash_list(t, tail(hashes), hash, i-1);
        }
    }
  }

  lemma void pred_mapping_drop_key<kt>(list<void*> kps, list<int> bbs,
                                       list<option<kt> > ks, int index)
  requires pred_mapping(kps, bbs, ?keyp, ks) &*&
           0 <= index &*& index < length(bbs) &*&
           nth(index, ks) == some(?k);
  ensures pred_mapping(kps, update(index, 0, bbs), keyp, update(index, none, ks)) &*&
          [0.25]keyp(nth(index, kps), k);
  {
    open pred_mapping(kps, bbs, keyp, ks);
    switch(bbs) {
      case nil:
      case cons(h,t):
        if (index != 0) {
          pred_mapping_drop_key(tail(kps), t, tail(ks), index - 1);
        }
    }
    close pred_mapping(kps, update(index, 0, bbs), keyp, update(index, none, ks));
  }

  lemma void
  buckets_remove_key_chains_still_start_on_hash_rec<kt>
    (list<bucket<kt> > buckets, unsigned capacity, kt k,
     fixpoint (kt,unsigned) hash, int start)
  requires true == key_chains_start_on_hash_fp(buckets, start, capacity, hash);
  ensures true == key_chains_start_on_hash_fp
                    (buckets_remove_key_fp(buckets, k), start, capacity, hash);
  {
    switch(buckets) {
      case nil:
      case cons(h,t):
        switch(h) { case bucket(chains):
          forall_filter((has_given_hash_fp)(hash, start, capacity),
                        (not_this_key_pair_fp)(k),
                        chains);
          buckets_remove_key_chains_still_start_on_hash_rec
            (t, capacity, k, hash, start+1);
        }
    }
  }

  lemma void
  buckets_remove_key_chains_still_start_on_hash<kt>
    (list<bucket<kt> > buckets, unsigned capacity, kt k,
     fixpoint (kt,unsigned) hash)
  requires true == key_chains_start_on_hash_fp(buckets, 0, capacity, hash);
  ensures true == key_chains_start_on_hash_fp
                    (buckets_remove_key_fp(buckets, k), 0, capacity, hash);
  {
    buckets_remove_key_chains_still_start_on_hash_rec
      (buckets, capacity, k, hash, 0);
  }
  @*/

static
unsigned find_key_remove_chain/*@ <kt> @*/(int* busybits, void** keyps,
                                           unsigned* k_hashes, int* chns,
                                           void* keyp,
                                           unsigned key_hash,
                                           unsigned key_size, unsigned capacity)
/*@ requires hmapping<kt>(?kpr, ?hsh, capacity, busybits, ?kps, k_hashes, ?hm) &*&
             buckets_ks_insync(chns, capacity, ?buckets, hsh, hmap_ks_fp(hm)) &*&
             pointers(keyps, capacity, kps) &*&
             [?kfr]kpr(keyp, ?k) &*&
             hsh(k) == key_hash &*&
             [?f]is_map_keys_equality<kt>(eq, kpr) &*&
             true == hmap_exists_key_fp(hm, k)
             is_pow2(capacity, N31) != none; @*/
/*@ ensures hmapping<kt>(kpr, hsh, capacity,
                         busybits, kps, k_hashes,
                         hmap_rem_key_fp(hm, hmap_find_key_fp(hm, k))) &*&
            buckets_ks_insync(chns, capacity,
                                buckets_remove_key_fp(buckets, k), hsh,
                                hmap_ks_fp(hmap_rem_key_fp
                                             (hm, hmap_find_key_fp(hm, k)))) &*&
            pointers(keyps, capacity, kps) &*&
            [kfr]kpr(keyp, k) &*&
            [f]is_map_keys_equality<kt>(eq, kpr) &*&
            result == hmap_find_key_fp(hm, k); @*/
{
  //@ open hmapping(_, _, _, _, _, _, hm);
  //@ open buckets_ks_insync(chns, capacity, buckets, hsh, hmap_ks_fp(hm));
  //@ assert ints(chns, capacity, ?chnlist);
  //@ assert pred_mapping(kps, ?bbs, kpr, ?ks);
  //@ assert hm == hmap(ks, ?khs);
  unsigned i = 0;
  unsigned start = loop(key_hash, capacity);
  //@ buckets_keys_chns_same_len(buckets);
  //@ assert true == hmap_exists_key_fp(hm, k);
  //@ assert start == loop_fp(hsh(k), capacity);
  //@ key_is_contained_in_the_bucket(buckets, capacity, hsh, k);
  //@ buckets_remove_add_one_chain(buckets, start, k);
  //@ loop_bijection(start, capacity);
  for (; i < capacity; ++i)
    /*@ invariant pred_mapping(kps, bbs, kpr, ks) &*&
                  ints(busybits, capacity, bbs) &*&
                  uints(k_hashes, capacity, khs) &*&
                  ints(chns, capacity, chnlist) &*&
                  pointers(keyps, capacity, kps) &*&
                  0 <= i &*& i <= capacity &*&
                  [f]is_map_keys_equality<kt>(eq, kpr) &*&
                  [kfr]kpr(keyp, k) &*&
                  hsh(k) == key_hash &*&
                  true == hash_list(ks, khs, hsh) &*&
                  *keyp_out |-> _ &*&
                  ks == buckets_get_keys_fp(buckets) &*&
                  i <= buckets_get_chain_fp(buckets, k, start) &*&
                  chnlist ==
                    add_partial_chain_fp
                      (loop_fp(start + i, capacity),
                       buckets_get_chain_fp(buckets, k, start) - i,
                       buckets_get_chns_fp(buckets_remove_key_fp(buckets,
                                                                 k))) &*&
                  true == up_to(nat_of_int(i),
                                (byLoopNthProp)(ks, (neq)(some(k)),
                                                capacity, start));
    @*/
    //@ decreases capacity - i;
  {
    //@ pred_mapping_same_len(bbs, ks);
    unsigned index = loop(start + i, capacity);
    int bb = busybits[index];
    unsigned kh = k_hashes[index];
    int chn = chns[index];
    void* kp = keyps[index];
    if (bb != 0 && kh == key_hash) {
      //@ close pred_mapping(nil, nil, kpr, nil);
      //@ extract_pred_for_key(nil, nil, nil, index, bbs, ks);
      //@ append_nil(reverse(take(index, kps)));
      //@ append_nil(reverse(take(index, bbs)));
      //@ append_nil(reverse(take(index, ks)));
      if (generic_eq(kp, keyp, key_size)) {
        /*@ recover_pred_mapping(kps, bbs, ks, index); @*/
        //@ hmap_find_this_key(hm, index, k);
        busybits[index] = 0;
        //@ hmap_rem_preserves_no_dups(ks, index);
        //@ hmap_rem_preserves_hash_list(ks, khs, hsh, index);
        //@ pred_mapping_drop_key(kps, bbs, ks, index);
        /*@ close hmapping<kt>(kpr, hsh, capacity, busybits, kps, k_hashes,
                               hmap_rem_key_fp(hm, hmap_find_key_fp(hm, k)));
          @*/
        //@ assert nth(index, hmap_ks_fp(hm)) == some(k);
        //@ chns_after_partial_chain_ended(buckets, k, start, i, capacity);
        //@ buckets_remove_key_still_ok(buckets, k);
        //@ buckets_rm_key_get_keys(buckets, k);
        /*@ buckets_remove_key_chains_still_start_on_hash
              (buckets, capacity, k, hsh);
          @*/
        /*@ close buckets_ks_insync(chns, capacity,
                                      buckets_remove_key_fp(buckets, k),
                                      hsh,
                                      update(index_of(some(k), ks), none, ks));
          @*/
        return index;
      }
      //@ recover_pred_mapping(kps, bbs, ks, index);
    } else {
      //@ assert(length(ks) == capacity);
      //@ if (bb != 0) no_hash_no_key(ks, khs, k, index, hsh);
      //@ if (bb == 0) no_bb_no_key(ks, bbs, index);
    }
    //@ buckets_remove_key_same_len(buckets, k);
    //@ buckets_keys_chns_same_len(buckets_remove_key_fp(buckets, k));
    //@ assert nth(index, ks) != some(k);
    //@ buckets_get_chain_longer(buckets, start, i, k, capacity);
    //@ assert buckets_get_chain_fp(buckets, k, start) != i;
    //@ buckets_get_chns_nonneg(buckets_remove_key_fp(buckets, k));
    /*@ add_part_chn_gt0(index, buckets_get_chain_fp(buckets, k, start) - i,
                         buckets_get_chns_fp
                           (buckets_remove_key_fp(buckets, k))); @*/
    //@ assert 0 < nth(index, chnlist);
    //@ assert 0 < chn;
    //@ integer_limits(&chn);
    chns[index] = chn - 1;
    //@ assert(nth(index, ks) != some(k));
    //@ assert(true == neq(some(k), nth(index, ks)));
    //@ assert(true == neq(some(k), nth(loop_fp(i+start,capacity), ks)));
    //@ assert(nat_of_int(i+1) == succ(nat_of_int(i)));
    //@ buckets_keys_chns_same_len(buckets);
    //@ assert length(buckets) == capacity;
    //@ assert length(chnlist) == length(buckets);
    //@ buckets_remove_key_same_len(buckets, k);
    //@ buckets_keys_chns_same_len(buckets_remove_key_fp(buckets, k));
    /*@ add_partial_chain_same_len
          (start + i, buckets_get_chain_fp(buckets, k, start) - i,
           buckets_get_chns_fp(buckets_remove_key_fp(buckets, k)));
      @*/
    //@ loop_fixp(start + i, capacity);
    //@ buckets_ok_get_chain_bounded(buckets, k, start);
    /*@ remove_one_cell_from_partial_chain
          (chnlist, loop_fp(start + i, capacity),
           buckets_get_chain_fp(buckets, k, start) - i,
           buckets_get_chns_fp(buckets_remove_key_fp(buckets, k)),
           capacity);
      @*/
    /*@ assert ints(chns, capacity,
      update(index, nth(index, chnlist) - 1,
      add_partial_chain_fp
      (loop_fp(start + i, capacity),
      buckets_get_chain_fp(buckets, k, start) - i,
      buckets_get_chns_fp(buckets_remove_key_fp(buckets, k)))));
      @*/
    /*@ assert ints(chns, capacity,
      add_partial_chain_fp
      (loop_fp(loop_fp(start + i, capacity) + 1, capacity),
      buckets_get_chain_fp(buckets, k, start) - i - 1,
      buckets_get_chns_fp(buckets_remove_key_fp(buckets, k))));
      @*/
    //@ inc_modulo_loop(start + i, capacity);
    //@ assert true == (loop_fp(loop_fp(start + i, capacity) + 1, capacity) == loop_fp(start + i + 1, capacity));
    /*@ chnlist = add_partial_chain_fp
                   (loop_fp(start + i + 1, capacity),
                    buckets_get_chain_fp(buckets, k, start) - i - 1,
                    buckets_get_chns_fp(buckets_remove_key_fp(buckets,
                                                              k)));
                                                              @*/
    /*@ assert ints(chns, capacity,
        add_partial_chain_fp
          (loop_fp(start + i + 1, capacity),
           buckets_get_chain_fp(buckets, k, start) - i - 1,
           buckets_get_chns_fp(buckets_remove_key_fp(buckets, k))));
           @*/
  }
  //@ pred_mapping_same_len(bbs, ks);
  //@ by_loop_for_all(ks, (neq)(some(k)), start, capacity, nat_of_int(capacity));
  //@ no_key_found(ks, k);
  //@ close hmapping<kt>(kpr, hsh, capacity, busybits, kps, k_hashes, hm);

  //@ assert false;
  return -1;
}

/*@







  @*/


/*@
  fixpoint hmap<kt> empty_hmap_fp<kt>(unsigned capacity, list<unsigned> hashes) {
    return hmap(none_list_fp<kt>(nat_of_int(capacity)),
                hashes);
  }

  fixpoint bool rec_props<kt>(fixpoint (kt,int,bool) prop,
                              list<pair<kt,int> > recs) {
    switch (recs) {
      case nil: return true;
      case cons(rec,tail):
        return true == prop(fst(rec),snd(rec)) &&
                       rec_props(prop, tail);
    }
  }


  fixpoint bool no_dup_keys<kt,vt>(list<pair<kt,vt> > m) {
    switch(m) {
      case nil:
        return true;
      case cons(h,t):
        return (false == map_has_fp(t, fst(h))) && no_dup_keys(t);
    }
  }

  lemma void remove_unique_no_dups<kt,vt>(list<pair<kt,vt> > m,
                                          pair<kt,vt> kv)
  requires false == map_has_fp(remove(kv, m), fst(kv));
  ensures no_dup_keys(m) == no_dup_keys(remove(kv, m));
  {
    switch(m) {
      case nil: return;
      case cons(h,t):
        if (h == kv) {
          assert(remove(kv, m) == t);
        } else {
          remove_unique_no_dups(t, kv);
          assert(remove(kv, m) == cons(h, remove(kv, t)));
          assert(m == cons(h,t));
          if (no_dup_keys(remove(kv,m))) {
            assert(true == no_dup_keys(t));
            assert(false == map_has_fp(remove(kv, t), fst(h)));
            map_remove_unrelated_key(t, fst(h), kv);
            assert(false == map_has_fp(t, fst(h)));
            assert(true == no_dup_keys(m));
          } else {
            if (map_has_fp(remove(kv,t),fst(h))) {
              map_remove_unrelated_key(t, fst(h), kv);
              assert(true == map_has_fp(t, fst(h)));
            } else {
              assert(false == no_dup_keys(remove(kv,t)));
            }
          }
        }
    }
  }

  lemma void hmap2map_no_key<kt,vt>(list<option<kt> > ks,
                                    list<pair<kt,vt> > m,
                                    kt key)
  requires key_vals(ks, ?va, m) &*& false == mem(some(key), ks);
  ensures key_vals(ks, va, m) &*& false == map_has_fp(m, key);
  {
    open key_vals(ks, va, m);
    switch(ks) {
      case nil:
        break;
      case cons(h,t):
        if (h == none) {
          hmap2map_no_key(t, m, key);
        } else {
          hmap2map_no_key(t, remove(pair(get_some(h), head(va)), m), key);
          map_remove_unrelated_key(m, key, pair(get_some(h), head(va)));
        }
    }
    close key_vals(ks, va, m);
  }

  lemma void map_no_dups<kt,vt>(list<option<kt> > ks, list<pair<kt,vt> > m)
  requires key_vals(ks, ?val_arr, m) &*& true == opt_no_dups(ks);
  ensures key_vals(ks, val_arr, m) &*& true == no_dup_keys(m);
  {
    open key_vals(ks, val_arr, m);
    switch(ks) {
      case nil:
        break;
      case cons(h,t):
        if (h == none) {
          map_no_dups(t, m);
        } else {
          map_no_dups(t, remove(pair(get_some(h), head(val_arr)), m));
          hmap2map_no_key(t, remove(pair(get_some(h), head(val_arr)), m),
                          get_some(h));
          remove_unique_no_dups(m, pair(get_some(h), head(val_arr)));
        }
    }
    close key_vals(ks, val_arr, m);
  }

  lemma void map_extract_recp<kt>(list<pair<kt,int> > m, kt k,
                                  fixpoint(kt,int,bool) prop)
  requires true == rec_props(prop, m) &*& true == map_has_fp(m, k);
  ensures true == prop(k, map_get_fp(m, k));
  {
    switch(m) {
      case nil: return;
      case cons(h,t):
        if (fst(h) == k) {
        } else {
          map_extract_recp(t, k, prop);
        }
    }
  }
  @*/

int map_impl_get/*@ <kt> @*/(int* busybits, void** keyps,
                             unsigned* k_hashes, int* chns,
                             int* values,
                             void* keyp,
                             unsigned hash, int* value,
                             unsigned key_size, unsigned capacity)
/*@ requires mapping<kt>(?m, ?addrs, ?kp, ?recp, ?hsh, capacity, busybits,
                         keyps, k_hashes, chns, values) &*&
             [?fk]kp(keyp, ?k) &*&
             [?fr]is_map_keys_equality(eq, kp) &*&
             hsh(k) == hash &*&
             *value |-> ?v &*&
             is_pow2(capacity, N31) != none; @*/
/*@ ensures mapping<kt>(m, addrs, kp, recp, hsh, capacity, busybits,
                        keyps, k_hashes, chns, values) &*&
            [fk]kp(keyp, k) &*&
            [fr]is_map_keys_equality(eq, kp) &*&
            (map_has_fp(m, k) ?
             (result == 1 &*&
              *value |-> ?nv &*&
              nv == map_get_fp(m, k) &*&
              true == recp(k, nv)):
             (result == 0 &*&
              *value |-> v)); @*/
{
  //@ open mapping(m, addrs, kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
  //@ open hmapping(kp, hsh, capacity, busybits, ?kps, k_hashes, ?hm);
  //@ close hmapping(kp, hsh, capacity, busybits, kps, k_hashes, hm);
  int index = find_key(keyps, busybits, k_hashes, chns,
                       keyp, hash, key_size, capacity);
  //@ hmap_exists_iff_map_has(hm, m, k);
  if (-1 == index)
  {
    //@ close mapping(m, addrs, kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
    return 0;
  }
  //@ hmapping_ks_capacity(hm, capacity);
  //@ assert(index < capacity);
  //@ assert(ints(values, capacity, ?val_arr));
  *value = values[index];
  //@ hmap_find_returns_the_key(hm, val_arr, m, k);
  //@ map_extract_recp(m, k, recp);
  //@ close mapping(m, addrs, kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
  return 1;
}


/*@
  lemma void ks_map_size<kt>(list<option<kt> > ks, list<pair<kt,int> > m)
  requires key_vals(ks, ?va, m);
  ensures key_vals(ks, va, m) &*&
          ks_size_fp(ks) == map_size_fp(m);
  {
    open key_vals(ks, va, m);
    switch(ks) {
      case nil:
      case cons(h,t):
        if (h == none) {
          ks_map_size(t, m);
        } else {
          ks_map_size(t, remove(pair(get_some(h), head(va)), m));
        }

    }
    close key_vals(ks, va, m);
  }

  lemma void hmap_map_size<kt>(hmap<kt> hm, list<pair<kt,int> > m)
  requires key_vals(hmap_ks_fp(hm), ?va, m);
  ensures key_vals(hmap_ks_fp(hm), va, m) &*&
          hmap_size_fp(hm) == map_size_fp(m);
  {
    ks_map_size(hmap_ks_fp(hm), m);
  }




//took 275m and counting
  @*/

void map_impl_put/*@ <kt> @*/(int* busybits, void** keyps,
                              unsigned* k_hashes, int* chns,
                              int* values,
                              void* keyp, unsigned hash, int value,
                              unsigned capacity)
/*@ requires mapping<kt>(?m, ?addrs, ?kp, ?recp, ?hsh, capacity, busybits,
                         keyps, k_hashes, chns, values) &*&
             [0.25]kp(keyp, ?k) &*& true == recp(k, value) &*&
             hsh(k) == hash &*&
             false == map_has_fp(m, k) &*&
             map_size_fp(m) < capacity &*&
             is_pow2(capacity, N31) != none; @*/
/*@ ensures true == recp(k, value) &*&
            mapping<kt>(map_put_fp(m, k, value),
                        map_put_fp(addrs, k, keyp),
                        kp, recp,
                        hsh,
                        capacity, busybits,
                        keyps, k_hashes, chns, values); @*/
{
  //@ open mapping(m, addrs, kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
  //@ open hmapping(kp, hsh, capacity, busybits, ?kps, k_hashes, ?hm);
  unsigned start = loop(hash, capacity);
  //@ close hmapping(kp, hsh, capacity, busybits, kps, k_hashes, hm);
  //@ hmap_map_size(hm, m);
  //@ assert buckets_ks_insync(chns, capacity, ?buckets, hsh, ?ks);
  //@ assert ks == hmap_ks_fp(hm);
  //@ open buckets_ks_insync(chns, capacity, buckets, hsh, ks);
  //@ assert ks == buckets_get_keys_fp(buckets);
  //@ buckets_keys_chns_same_len(buckets);
  //@ close buckets_ks_insync(chns, capacity, buckets, hsh, ks);
  //@ assert length(buckets) == capacity;
  unsigned index = find_empty(busybits, chns, start, capacity);


  //@ hmapping_ks_capacity(hm, capacity);
  //@ open hmapping(kp, hsh, capacity, busybits, kps, k_hashes, hm);
  //@ assert pred_mapping(kps, ?bbs, kp, ks);
  //@ put_keeps_pred_mapping(kps, bbs, kp, ks, index, keyp, k);
  //@ hmap_exists_iff_map_has(hm, m, k);
  //@ put_preserves_no_dups(ks, index, k);
  //@ assert(hm == hmap(ks, ?khs));
  //@ assert(ints(values, capacity, ?vals));
  //@ hmap_coherent_hash_update(ks, khs, hsh, index, k, hash);
  busybits[index] = 1;
  keyps[index] = keyp;
  k_hashes[index] = hash;
  values[index] = value;
  /*@ close hmapping(kp, hsh, capacity, busybits, update(index, keyp, kps),
                     k_hashes, hmap_put_key_fp(hm, index, k, hash));
    @*/
  //@ coherent_put_preserves_key_vals(hmap_ks_fp(hm), vals, m, index, k, value);
  //@ coherent_put_preserves_key_vals(hmap_ks_fp(hm), kps, addrs, index, k, keyp);
  // @ assert capacity == length(chns);
  //@ assert length(hmap_ks_fp(hm)) == capacity;
  //@ assert length(ks) == capacity;
  //@ assert length(ks) == length(buckets);
  //@ no_key_in_ks_no_key_in_buckets(buckets, k);
  /*@ buckets_ks_put_key_insync(chns, capacity,
                                hsh,
                                start, index, k,
                                ks);
    @*/
  /*@ close mapping(map_put_fp(m, k, value), map_put_fp(addrs, k, keyp),
                    kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
    @*/
}

/*@
  lemma void hmap_rem_preserves_pred_mapping<kt>(list<char*> kps, list<int> bbs,
                                                 predicate (void*;kt) keyp,
                                                 list<option<kt> > ks,
                                                 int i)
  requires pred_mapping(kps, bbs, keyp, ks) &*&
           0 <= i &*& i < length(ks) &*& nth(i, ks) == some(?k);
  ensures pred_mapping(kps, update(i, 0, bbs), keyp, update(i, none, ks)) &*&
          [0.25]keyp(nth(i, kps), k);
  {
    open pred_mapping(kps, bbs, keyp, ks);
    switch(bbs) {
      case nil: break;
      case cons(bbh, bbt):
        cons_head_tail(ks);
        cons_head_tail(kps);
        if (i == 0) {
        } else {
          hmap_rem_preserves_pred_mapping(tail(kps), bbt, keyp, tail(ks), i-1);
          nth_cons(i, tail(ks), head(ks));
          nth_cons(i, tail(kps), head(kps));
        }
    }
    close pred_mapping(kps, update(i, 0, bbs), keyp, update(i, none, ks));
  }
  @*/

/*@
  lemma void map_erase_preserves_rec_props<kt>(list<pair<kt,int> > m,
                                               fixpoint(kt,int,bool) recp,
                                               kt k)
  requires true == rec_props(recp, m);
  ensures true == rec_props(recp, map_erase_fp(m, k));
  {
    switch(m) {
      case nil:
      case cons(h,t):
        if (fst(h) == k) {
        } else {
          map_erase_preserves_rec_props(t, recp, k);
        }
    }
  }
  @*/

/*@
  lemma void map_has_not_mem_not<kt,vt>(list<pair<kt,vt> > m,
                                        kt k, vt v)
  requires false == map_has_fp(m, k);
  ensures false == mem(pair(k,v), m);
  {
    switch(m){
      case nil: break;
      case cons(h,t):
        map_has_not_mem_not(t, k, v);
    }
  }

  lemma void map_no_dups_has_that_pair<kt,vt>(pair<kt,vt> mh,
                                              list<pair<kt,vt> > mt,
                                              kt k, vt v)
  requires true == no_dup_keys(cons(mh,mt)) &*&
           true == mem(pair(k,v), cons(mh,mt)) &*&
           fst(mh) == k;
  ensures mh == pair(k,v);
  {
    if (mh != pair(k,v)) {
      assert(false == map_has_fp(mt, fst(mh)));
      map_has_not_mem_not(mt, k, v);
    }
  }

  lemma void map_erase_that_key<kt,vt>(list<pair<kt,vt> > m,
                                       kt k, vt v)
  requires true == no_dup_keys(m) &*& true == mem(pair(k, v), m);
  ensures map_erase_fp(m, k) == remove(pair(k, v), m);
  {
    switch(m) {
      case nil: break;
      case cons(h,t):
        if (fst(h) == k) {
          map_no_dups_has_that_pair(h, t, k, v);
          assert(h == pair(k,v));
        } else {
          map_erase_that_key(t, k, v);
        }
    }
  }

  lemma void map_erase_unrelated_key<kt,vt>(list<pair<kt,vt> > m,
                                             pair<kt,vt> kv1, kt k2)
  requires fst(kv1) != k2;
  ensures mem(kv1, m) == mem(kv1, map_erase_fp(m, k2)) &*&
          remove(kv1, map_erase_fp(m, k2)) == map_erase_fp(remove(kv1, m), k2);
  {
    switch(m) {
      case nil: break;
      case cons(h,t):
        if (h == kv1) {
        } else {
          if (fst(h) == k2) {
          } else {
            map_erase_unrelated_key(t, kv1, k2);
          }
        }
    }
  }

  lemma void map_erase_remove_unrelated<kt>(list<pair<kt,int> > m,
                                            pair<kt,int> kv1, kt k2)
  requires fst(kv1) != k2;
  ensures remove(kv1, map_erase_fp(m, k2)) == map_erase_fp(remove(kv1, m), k2);
  {
    switch(m) {
      case nil: break;
      case cons(h,t):
        if (h == kv1) {
        } else {
          if (fst(h) == k2) {
          } else {
            map_erase_remove_unrelated(t, kv1, k2);
          }
        }
    }
  }

  lemma void ks_rem_map_erase_coherent<kt,vt>(list<option<kt> > ks,
                                              list<pair<kt,vt> > m,
                                              int i, kt k)
  requires key_vals(ks, ?vals, m) &*& nth(i, ks) == some(k) &*&
           true == no_dup_keys(m) &*& true == opt_no_dups(ks) &*&
           0 <= i &*& i < length(ks);
  ensures key_vals(update(i, none, ks), vals, map_erase_fp(m, k));
  {
    switch(ks) {
      case nil:
        open key_vals(ks, vals, m);
        close key_vals(update(i, none, ks), vals, map_erase_fp(m, k));
        break;
      case cons(h,t):
        open key_vals(ks, vals, m);
        if (i == 0) {
          tail_of_update_0(ks, none);
          assert(h == some(k));
          assert(true == mem(pair(k, head(vals)), m));
          map_erase_that_key(m, k, head(vals));
          assert(map_erase_fp(m, k) == remove(pair(k, head(vals)), m));
        } else {
          if (h == none) {
            ks_rem_map_erase_coherent(t, m, i-1, k);
          } else {
            hmap2map_no_key(t, remove(pair(get_some(h), head(vals)), m),
                            get_some(h));
            remove_unique_no_dups(m, pair(get_some(h), head(vals)));
            ks_rem_map_erase_coherent(t, remove(pair(get_some(h),
                                                     head(vals)), m),
                                      i-1, k);

            map_erase_unrelated_key(m, pair(get_some(h), head(vals)), k);
            update_tail_tail_update(h, t, i, none);
          }
        }
        close key_vals(update(i, none, ks), vals, map_erase_fp(m, k));
    }
  }

  lemma void hmap_ks_hmap_rm<kt>(hmap<kt> hm, int i)
  requires true;
  ensures hmap_ks_fp(hmap_rem_key_fp(hm, i)) == update(i, none, hmap_ks_fp(hm));
  {
    switch(hm) {
      case hmap(ks, khs): break;
    }
  }

  lemma void hmap_rem_map_erase_coherent<kt,vt>(hmap<kt> hm,
                                                list<pair<kt, vt> > m,
                                                int i, kt k)
  requires key_vals(hmap_ks_fp(hm), ?vals, m) &*&
           nth(i, hmap_ks_fp(hm)) == some(k) &*&
           true == opt_no_dups(hmap_ks_fp(hm)) &*&
           0 <= i &*& i < length(hmap_ks_fp(hm));
  ensures key_vals(hmap_ks_fp(hmap_rem_key_fp(hm, i)),
                   vals, map_erase_fp(m, k));
  {
     map_no_dups(hmap_ks_fp(hm), m);
     hmap_ks_hmap_rm(hm, i);
     ks_rem_map_erase_coherent(hmap_ks_fp(hm), m, i, k);
  }
  @*/

/*@
  lemma void map_erase_hasnt<kt,vt>(list<pair<kt,vt> > m, kt k)
  requires true == no_dup_keys(m);
  ensures false == map_has_fp(map_erase_fp(m, k), k);
  {
    switch(m) {
      case nil:
      case cons(h,t):
        switch(h) { case pair(key,value):
          if (key != k) map_erase_hasnt(t, k);
        }
    }
  }
  @*/


void map_impl_erase/*@ <kt> @*/(int* busybits, void** keyps,
                                unsigned* k_hashes, int* chns,
                                void* keyp,
                                unsigned hash, unsigned key_size, unsigned capacity)
/*@ requires mapping<kt>(?m, ?addrs, ?kp, ?recp, ?hsh, capacity, busybits,
                         keyps, k_hashes, chns, ?values) &*&
             [?fk]kp(keyp, ?k) &*&
             [?fr]is_map_keys_equality<kt>(eq, kp) &*&
             hsh(k) == hash &*&
             true == map_has_fp(m, k) &*&
             is_pow2(capacity, N31) != none; @*/
/*@ ensures [fk]kp(keyp, k) &*&
            [fr]is_map_keys_equality<kt>(eq, kp) &*&
            false == map_has_fp(map_erase_fp(m, k), k) &*&
            mapping<kt>(map_erase_fp(m, k), map_erase_fp(addrs, k),
                        kp, recp, hsh,
                        capacity, busybits, keyps, k_hashes, chns, values); @*/
{
  //@ open mapping(m, addrs, kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
  //@ open hmapping(kp, hsh, capacity, busybits, ?kps, k_hashes, ?hm);
  //@ close hmapping(kp, hsh, capacity, busybits, kps, k_hashes, hm);
  //@ map_no_dups(hmap_ks_fp(hm), m);
  //@ map_erase_hasnt(m, k);
  //@ hmap_exists_iff_map_has(hm, m, k);
  find_key_remove_chain(busybits, keyps, k_hashes, chns,
                        keyp, hash, key_size, capacity);
  //@ hmap_exists_iff_map_has(hm, m, k);
  // @ hmapping_ks_capacity(hm, capacity);
  // @ assert(index < capacity);
  // @ open hmapping(kp, hsh, capacity, busybits, kps, k_hashes, hm);
  // @ assert(pred_mapping(kps, ?bbs, kp, ?ks));
  // @ assert(ints(k_hashes, capacity, ?khs));
  //@ hmap_find_returns_the_key(hm, kps, addrs, k);
  //@ mem_nth_index_of(some(k), hmap_ks_fp(hm));
  // @ hmap_rem_preserves_pred_mapping(kps, bbs, kp, ks, index);
  // @ hmap_rem_preserves_no_dups(ks, index);
  // @ hmap_rem_preserves_hash_list(ks, khs, hsh, index);
  // @ close hmapping(kp, hsh, capacity, busybits, kps, k_hashes, hmap_rem_key_fp(hm, index));
  //@ map_erase_preserves_rec_props(m, recp, k);
  //@ hmap_rem_map_erase_coherent(hm, m, hmap_find_key_fp(hm,k), k);
  //@ hmap_rem_map_erase_coherent(hm, addrs, hmap_find_key_fp(hm,k), k);
  /*@ close mapping(map_erase_fp(m, k), map_erase_fp(addrs, k),
                    kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
    @*/
}

/*@
  fixpoint bool nonzero(int x) { return x != 0; }

  lemma void add_bit_to_nonzero_count(list<int> bbs, int i, int s)
  requires s == count(take(i, bbs), nonzero) &*& 0 <= i &*& i < length(bbs);
  ensures nth(i, bbs) == 0 ?
           s == count(take(i+1, bbs), nonzero) :
           s+1 == count(take(i+1, bbs), nonzero);
  {
    switch(bbs) {
      case nil: break;
      case cons(h,t):
        if (i == 0) {
        } else {
          if (h == 0) {
            add_bit_to_nonzero_count(t, i-1, s);
          } else {
            add_bit_to_nonzero_count(t, i-1, s-1);
          }
        }
    }
  }

  lemma void nonzero_count_is_ks_size<kt>(list<int> bbs, list<option<kt> > ks)
  requires pred_mapping(?kps, bbs, ?pred, ks);
  ensures pred_mapping(kps, bbs, pred, ks) &*&
          count(bbs, nonzero) == ks_size_fp(ks);
  {
    open pred_mapping(kps, bbs, pred, ks);
    switch(bbs) {
      case nil: break;
      case cons(h,t):
        nonzero_count_is_ks_size(t, tail(ks));
    }
    close pred_mapping(kps, bbs, pred, ks);
  }
  @*/

unsigned map_impl_size/*@ <kt> @*/(int* busybits, unsigned capacity)
/*@ requires mapping<kt>(?m, ?addrs, ?kp, ?recp, ?hsh, capacity, busybits,
                         ?keyps, ?k_hashes, ?chns, ?values); @*/
/*@ ensures mapping<kt>(m, addrs, kp, recp, hsh, capacity, busybits,
                        keyps, k_hashes, chns, values) &*&
            result == map_size_fp(m);@*/
{
  //@ open mapping(m, addrs, kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
  //@ open hmapping(kp, hsh, capacity, busybits, ?kps, k_hashes, ?hm);
  //@ assert ints(busybits, capacity, ?bbs);
  //@ assert pointers(keyps, capacity, kps);
  //@ assert uints(k_hashes, capacity, ?khs);
  //@ assert pred_mapping(kps, bbs, kp, ?ks);
  unsigned s = 0;
  unsigned i = 0;
  for (; i < capacity; ++i)
    /*@ invariant 0 <= i &*& i <= capacity &*&
                  0 < capacity &*& 2*capacity < INT_MAX &*&
                  ints(busybits, capacity, bbs) &*&
                  pointers(keyps, capacity, kps) &*&
                  uints(k_hashes, capacity, khs) &*&
                  pred_mapping(kps, bbs, kp, ks) &*&
                  true == opt_no_dups(ks) &*&
                  true == hash_list(ks, khs, hsh) &*&
                  hm == hmap(ks, khs) &*&
                  count(take(i, bbs), nonzero) == s &*&
                  0 <= s &*& s <= i;
      @*/
    //@ decreases capacity - i;
  {
    //@ add_bit_to_nonzero_count(bbs, i, s);
    if (busybits[i] != 0)
    {
      ++s;
    }
  }
  //@ nonzero_count_is_ks_size(bbs, ks);
  //@ assert(s == hmap_size_fp(hm));
  //@ hmap_map_size(hm, m);
  //@ close hmapping(kp, hsh, capacity, busybits, kps, k_hashes, hm);
  //@ close mapping(m, addrs, kp, recp, hsh, capacity, busybits, keyps, k_hashes, chns, values);
  return s;
}

/*@
  lemma void map_get_keeps_recp<kt>(list<pair<kt,int> > m, kt k)
  requires mapping(m, ?addrs, ?kp, ?rp, ?hsh,
                   ?cap, ?bbs, ?kps, ?khs, ?chns, ?vals) &*&
           true == map_has_fp(m, k);
  ensures true == rp(k, map_get_fp(m, k)) &*&
          mapping(m, addrs, kp, rp, hsh,
                  cap, bbs, kps, khs, chns, vals);
  {
    open mapping(m, addrs, kp, rp, hsh, cap, bbs, kps, khs, chns, vals);
    map_extract_recp(m, k, rp);
    close mapping(m, addrs, kp, rp, hsh, cap, bbs, kps, khs, chns, vals);
  }
  @*/

/*@
  lemma void no_dup_keys_to_no_dups<kt>(list<pair<kt, int> > m)
  requires true;
  ensures no_dup_keys(m) == no_dups(map(fst, m));
  {
    switch(m) {
      case nil:
      case cons(h,t):
        switch(h) { case pair(key,ind):
          no_dup_keys_to_no_dups(t);
          map_has_to_mem(t, key);
        }
    }
  }
  @*/

/*@
  lemma void map_no_dup_keys<kt>(list<pair<kt, int> > m)
  requires mapping(m, ?addrs, ?kp, ?rp, ?hsh,
                   ?cap, ?bbs, ?kps, ?khs, ?chns, ?vals);
  ensures mapping(m, addrs, kp, rp, hsh,
                  cap, bbs, kps, khs, chns, vals) &*&
          true == no_dups(map(fst, m));
  {
    open mapping(m, addrs, kp, rp, hsh, cap, bbs, kps, khs, chns, vals);
    open hmapping(kp, hsh, cap, ?busybits, ?lkps, khs, ?hm);
    assert pred_mapping(lkps, ?lbbs, kp, ?ks);
    map_no_dups(ks, m);
    close hmapping(kp, hsh, cap, busybits, lkps, khs, hm);
    close mapping(m, addrs, kp, rp, hsh, cap, bbs, kps, khs, chns, vals);
    no_dup_keys_to_no_dups(m);
  }
  @*/