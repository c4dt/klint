#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "os/network.h"


// Initialize any necessary state, given the number of devices; returns true iff initialization succeeded.
bool nf_init(uint16_t devices_count);

// Handles a packet
void nf_handle(struct os_net_packet* packet);