#define XDP_MAIN_FUNC xdp_fw_prog
#include "compat/skeleton/xdp.h"

#include "uapi/linux/bpf.h"
#include "bpf/bpf_helpers.h"

extern struct bpf_map_def tx_port;
extern struct bpf_map_def flow_ctx_table;

bool nf_init(device_t devices_count)
{
	(void) devices_count;

	bpf_map_init(&tx_port, false);
	bpf_map_init(&flow_ctx_table, false);

	return true;
}