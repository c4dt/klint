#include "flowtable.h"

#include "os/memory.h"
#include "os/structs/dchain.h"
#include "os/structs/map.h"


struct flowtable
{
	struct flow* flows;
	struct os_map* flow_indexes;
	struct os_dchain* port_allocator;
	int64_t expiration_time;
	uint64_t max_flows;
	uint16_t start_port;
	uint8_t _padding[6];
};


struct flowtable* flowtable_init(uint16_t start_port, int64_t expiration_time, uint64_t max_flows)
{
	// TODO get rid of failures in DS contracts, somehow?
	struct os_map* flow_indexes = os_map_init(sizeof(struct flow), max_flows); // TODO: 2*max_flows because it's only a small amount of additional space for a lot more tput when near full
	struct os_dchain* port_allocator = os_dchain_init(max_flows);

	if ((flow_indexes == 0) | (port_allocator == 0)) {
		return 0;
	}

	struct flowtable* table = os_memory_init(1, sizeof(struct flowtable));
	table->flows = os_memory_init(max_flows, sizeof(struct flow));
	table->flow_indexes = flow_indexes;
	table->port_allocator = port_allocator;
	table->expiration_time = expiration_time;
	table->max_flows = max_flows;
	table->start_port = start_port;
	return table;
}

bool flowtable_get_internal(struct flowtable* table, int64_t time, struct flow* flow, uint16_t* out_port)
{
	uint64_t index;
	if (os_map_get(table->flow_indexes, flow, &index)) {
		os_dchain_refresh(table->port_allocator, time, index);
	} else {
		if (os_dchain_expire(table->port_allocator, time - table->expiration_time, &index)) {
			os_map_erase(table->flow_indexes, &(table->flows[index]));
		}

		if (!os_dchain_add(table->port_allocator, time, &index)) {
			return false;
		}

		table->flows[index] = *flow;
		os_map_put(table->flow_indexes, &(table->flows[index]), index);
	}

	*out_port = table->start_port + index;
	return true;
}

bool flowtable_get_external(struct flowtable* table, int64_t time, uint16_t port, struct flow* out_flow)
{
	uint64_t index = (uint16_t) (port - table->start_port);
	// Per its contract, we cannot call 'os_dchain_get' with an out-of-range index
	if (index >= table->max_flows) {
		return false;
	}

	time_t flow_time;
	if (!os_dchain_get(table->port_allocator, index, &flow_time) || time - table->expiration_time > flow_time) {
		return false;
	}

	os_dchain_refresh(table->port_allocator, time, index);
	*out_flow = table->flows[index];
	return true;
}