#include "flowtable.h"

#include "os/memory.h"
#include "structs/map.h"
#include "structs/pool.h"


struct flowtable
{
	struct flow* flows;
	struct os_map* flow_indexes;
	struct os_pool* port_allocator;
	size_t max_flows;
	uint16_t start_port;
	uint8_t _padding[6];
};


struct flowtable* flowtable_alloc(uint16_t start_port, time_t expiration_time, size_t max_flows)
{
	struct os_map* flow_indexes = os_map_alloc(sizeof(struct flow), max_flows);
	struct os_pool* port_allocator = os_pool_alloc(max_flows, expiration_time);
	struct flowtable* table = os_memory_alloc(1, sizeof(struct flowtable));
	table->flows = os_memory_alloc(max_flows, sizeof(struct flow));
	table->flow_indexes = flow_indexes;
	table->port_allocator = port_allocator;
	table->max_flows = max_flows;
	table->start_port = start_port;
	return table;
}

bool flowtable_get_internal(struct flowtable* table, time_t time, struct flow* flow, uint16_t* out_port)
{
	size_t index;
	if (os_map_get(table->flow_indexes, flow, &index)) {
		os_pool_refresh(table->port_allocator, time, index);
	} else {
		bool was_used;
		if (!os_pool_borrow(table->port_allocator, time, &index, &was_used)) {
			return false;
		}

		if (was_used) {
			os_map_remove(table->flow_indexes, &(table->flows[index]));
		}

		table->flows[index] = *flow;
		os_map_set(table->flow_indexes, &(table->flows[index]), index);
	}

	*out_port = table->start_port + index;
	return true;
}

bool flowtable_get_external(struct flowtable* table, time_t time, uint16_t port, struct flow* out_flow)
{
	size_t index = (uint16_t) (port - table->start_port);
	if (!os_pool_contains(table->port_allocator, time, index)) {
		return false;
	}

	os_pool_refresh(table->port_allocator, time, index);
	*out_flow = table->flows[index];
	return true;
}
