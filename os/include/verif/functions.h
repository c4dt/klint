#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef void foreach_index_function(size_t index, void* state);
void foreach_index(size_t length, foreach_index_function* func, void* state);

// The batch hint is optional but helps with latency
typedef bool foreach_index_forever_function(size_t index, void* state);
_Noreturn void foreach_index_forever(size_t length, size_t batch_hint, foreach_index_forever_function* func, void* state);

void foreach_index_set(size_t length, size_t* array, size_t value);

typedef uint32_t argmin_uint32_function(size_t index, void* state, uint32_t* out_arg);
uint32_t argmin_uint32(size_t length, argmin_uint32_function* func, void* state);