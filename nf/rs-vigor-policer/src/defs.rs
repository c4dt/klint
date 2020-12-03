use std::os::raw::c_char;

pub const OS_NET_ETHER_ADDR_SIZE: usize = 6;

pub type TimeT = i64;

#[repr(C)]
pub struct OsNetPacket {
    pub data: *mut u8,
    pub _reserved0: u64, // DPDK buf_iova
    pub _reserved1: u16, // DPDK data_off
    pub _reserved2: u16, // DPDK refcnt
    pub _reserved3: u16, // DPDK nb_segs
    pub device: u16,
    pub _reserved4: u64, // DPDK ol_flags
    pub _reserved5: u32, // DPDK packet_type
    pub _reserved6: u32, // DPDK pkt_len
    pub length: u16,
}

#[repr(C)]
pub struct OsNetEtherHeader {
    pub src_addr: [u8; OS_NET_ETHER_ADDR_SIZE],
    pub dst_addr: [u8; OS_NET_ETHER_ADDR_SIZE],
    pub ether_type: u16,
}

#[repr(C)]
pub struct OsNetIPv4Header {
    pub version_ihl: u8,
    pub type_of_service: u8,
    pub total_length: u16,
    pub packet_id: u16,
    pub fragment_offset: u16,
    pub time_to_live: u8,
    pub next_proto_id: u8,
    pub hdr_checksum: u16,
    pub src_addr: u32,
    pub dst_addr: u32,
}

#[repr(C)]
pub struct OsNetTcpUdpHeader {
    pub src_port: u16,
    pub dst_port: u16,
}

#[inline]
pub unsafe fn os_net_get_ether_header(packet: *mut OsNetPacket, out_ether_header: *mut *mut OsNetEtherHeader) -> bool {
    *out_ether_header = (*packet).data as *mut OsNetEtherHeader;
    true
}

#[inline]
pub unsafe fn os_net_get_ipv4_header(ether_header: *mut OsNetEtherHeader, out_ipv4_header: *mut *mut OsNetIPv4Header) -> bool {
    *out_ipv4_header = ether_header.offset(1) as *mut OsNetIPv4Header;
    u16::from_be((*ether_header).ether_type) == 0x0800
}

#[repr(C)]
pub struct OsMap {
    _private: [u8; 0],
}
#[repr(C)]
pub struct OsPool {
    _private: [u8; 0],
}

extern "C" {
    // OS API
    pub fn os_config_get_u16(name: *const c_char) -> u16;
    pub fn os_config_get_u64(name: *const c_char) -> u64;
    pub fn os_memory_alloc(count: usize, size: usize) -> *mut u8;
    pub fn os_clock_time() -> TimeT;
    pub fn os_net_transmit(
        packet: *mut OsNetPacket,
        device: u16,
        ether_header: *mut OsNetEtherHeader,
        ipv4_header: *mut OsNetIPv4Header,
        tcpudp_header: *mut OsNetTcpUdpHeader,
    );

    // Map API
    pub fn os_map_alloc(key_size: usize, capacity: usize) -> *mut OsMap;
    pub fn os_map_get(map: *mut OsMap, key_ptr: *mut u8, out_value: *mut *mut u8) -> bool;
    pub fn os_map_set(map: *mut OsMap, key_ptr: *mut u8, value: *mut u8);
    pub fn os_map_remove(map: *mut OsMap, key_ptr: *mut u8);

    // Pool API
    pub fn os_pool_alloc(size: usize) -> *mut OsPool;
    pub fn os_pool_borrow(pool: *mut OsPool, time: TimeT, out_index: *mut usize) -> bool;
    // pub fn os_pool_return(pool: *mut OsPool, index: usize);
    pub fn os_pool_refresh(pool: *mut OsPool, time: TimeT, index: usize);
    // pub fn os_pool_used(pool: *mut OsPool, index: usize, out_time: *mut TimeT) -> bool;
    pub fn os_pool_expire(pool: *mut OsPool, time: TimeT, out_index: *mut usize) -> bool;
}
