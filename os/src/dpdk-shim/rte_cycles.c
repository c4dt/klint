#include <rte_cycles.h>

#include "os/clock.h"


void rte_delay_us_sleep(unsigned int us)
{
	os_clock_sleep_us(us);
}

void (*rte_delay_us)(unsigned int) = rte_delay_us_sleep;
