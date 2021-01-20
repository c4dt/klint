#include <rte_atomic.h>

// TODO move this to arch/...
#define rte_compiler_barrier() do { asm volatile ("" : : : "memory"); } while(0)

void rte_io_rmb(void)
{
	rte_compiler_barrier();
}

void rte_io_wmb(void)
{
	rte_compiler_barrier();
}

void rte_wmb(void)
{
	rte_compiler_barrier();
}

void rte_smp_rmb(void)
{
	// OS ASSUMPTION: Single core
	// Nothing
}

void rte_smp_wmb(void)
{
	// OS ASSUMPTION: Single core
	// Nothing
}
