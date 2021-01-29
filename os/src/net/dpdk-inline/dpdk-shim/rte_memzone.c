#include <rte_memzone.h>

#include "os/fail.h"
#include "os/memory.h"

// TODO check if this file is really needed once everything works (and while we're at it let's check that for every function/define/typedef/... in dpdk-shim)


#define MAX_MEMZONES 32

static struct rte_memzone all_zones[MAX_MEMZONES];
static size_t zone_index;


const struct rte_memzone* rte_memzone_reserve(const char* name, size_t len, int socket_id, unsigned flags)
{
	return rte_memzone_reserve_aligned(name, len, socket_id, flags, os_memory_pagesize());
}

const struct rte_memzone* rte_memzone_reserve_aligned(const char* name, size_t len, int socket_id, unsigned flags, unsigned align)
{
	if ((flags & RTE_MEMZONE_SIZE_HINT_ONLY) == 0) {
		os_fail("Memzone reserve is too strict");
	}

	if (zone_index == MAX_MEMZONES) {
		os_fail("Ran out of storage for zones");
	}

	if (align > len) {
		len = align;
	}

	struct rte_memzone* zone = &(all_zones[zone_index]);

	zone->addr = os_memory_alloc(1, len);
	zone->phys_addr = os_memory_virt_to_phys(zone->addr);
	zone->len = len;
	zone->hugepage_sz = os_memory_pagesize();
	zone->socket_id = socket_id;
	zone->flags = flags;

	for (size_t n = 0; n < RTE_MEMZONE_NAMESIZE; n++) {
		zone->name[n] = name[n];
		if (name[n] == 0) {
			break;
		}
	}

	zone_index = zone_index + 1;
	return zone;
}

const struct rte_memzone* rte_memzone_lookup(const char* name)
{
	for (size_t z = 0; z < zone_index; z++) {
		for (size_t n = 0; n < RTE_MEMZONE_NAMESIZE; n++) {
			if (all_zones[z].name[n] != name[n]) {
				break;
			}
			if (name[n] == 0) {
				return &(all_zones[z]);
			}
		}
	}

	os_fail("Could not find zone");
}

int rte_memzone_free(const struct rte_memzone* mz)
{
	(void) mz;

	// No freeing
	return 0;
}