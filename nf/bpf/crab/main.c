#define XDP_MAIN_FUNC xdp_prog_simple
#include "compat/skeleton/xdp.h"

#include "os/memory.h"

extern struct bpf_map_def targets_map;
extern struct bpf_map_def macs_map;
extern struct bpf_map_def targets_count;
extern struct bpf_map_def cpu_rr_idx;

bool nf_init(device_t devices_count)
{
	(void) devices_count;

	bpf_map_init(&targets_map);
	bpf_map_init(&macs_map);
	bpf_map_init(&targets_count);
	bpf_map_init(&cpu_rr_idx);

	// CRAB assumes this
	if (devices_count == 0) {
		return false;
	}
	uint32_t k = 0;
	uint32_t v = devices_count;
	bpf_map_update_elem(&targets_count, &k, &v, 0);

	return true;
}