#include "os/init.h"

#include <rte_cycles.h>
#include <rte_debug.h>

#include "os/error.h"


// For clock.h
uint64_t freq_numerator;
uint64_t freq_denominator;

void os_init(void)
{
	freq_numerator = rte_get_tsc_hz();
	if (freq_numerator == 0) {
		rte_panic("Could not get TSC freq");
	}
	freq_denominator = 1000000000ull;
	while (freq_numerator % 10 == 0) {
		freq_numerator = freq_numerator / 10;
		freq_denominator = freq_denominator / 10;
	}
}