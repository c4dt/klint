#pragma once

#include "net/packet.h"


enum net_transmit_flags {
	UPDATE_ETHER_ADDRS = 1 << 0,
};

// Transmit the given packet on the given device, with the given flags
void net_transmit(struct net_packet* packet, uint16_t device, enum net_transmit_flags flags);

// Transmit the given packet unmodified to all devices except the packet's own
// TODO: This should not be necessary, it's only required because we can't properly deal with loops over devices during verification
void net_flood(struct net_packet* packet);