# This file is prefixed to all specifications.
# It contains core verification-related concepts, including the "_spec_wrapper" that is called by the verification engine.
# It "talks" to the outside world via the global __symbex__ variable.

from collections import namedtuple


# === Typing ===

def type_size(type):
    if isinstance(type, dict):
        return sum([type_size(v) for v in type.values()])
    if isinstance(type, str):
        global __symbex__
        return int(getattr(__symbex__.state.sizes, type))
    return int(type)


def type_wrap(value, type):
    if not isinstance(type, dict):
        return value
    result = {}
    offset = 0
    for (k, v) in self._type.items(): # Python preserves insertion order from 3.7 (3.6 for CPython)
        result[k] = value[type_size(v)+offset-1:offset]
        offset = offset + type_size(v)
    return result

def type_unwrap(value, type):
    if not isinstance(value, dict):
        return value
    if isinstance(type, dict):
        assert value.keys() == type.keys(), "please don't cast in weird ways"
        return value
    assert value != {}, "please don't use empty dicts"
    # almost a proxy, let's handle it here...
    result = None
    total_size = 0
    for v in value.values():
        if result is None:
            result = v
        else:
            result = v.concat(result)
    if result.size() < type_size(type):
        result = result.zero_extend(type_size(type) - result.size())
    return result


# === Spec 'built-in' functions ===

def exists(type, func):
    global __symbex__
    value = __symbex__.state.solver.BVS("exists_value", type_size(type))
    results = __symbex__.state.solver.eval_upto(func(value), 2)
    return results == [True]


# === Maps ===

class Map:
    def __init__(self, key_type, value_type, _state=None, _map=None):
        if _state is None:
            global __symbex__
            _state = __symbex__.state
        self._state = _state

        if _map is None:
            key_size = type_size(key_type)
            value_size = ... if value_type is ... else type_size(value_type)

            if value_type == "size_t":
                candidates = [m for (_, m) in self._state.maps.items() if "map_values_4" in str(m)]
            elif value_type == "uint64_t":
                candidates = [m for (_, m) in self._state.maps.items() if "pool_items_6" in str(m)]
            else:
                raise "oh"

            """candidates = []
            for (_, m) in self._state.maps.items():
                # Ignore maps that the user did not declare, i.e., fractions ones & the packet itself
                if ("fracs_" not in m.meta.name) & ("packet_" not in m.meta.name) & \
                  (m.meta.key_size >= key_size) & ((value_size is ...) | (m.meta.value_size == value_size)):
                    candidates.append(m)"""
            if len(candidates) == 0:
                raise Exception("No such map: " + str(key_type) + " -> " + str(value_type))
            _map = __choose__(candidates)

        self._map = _map
        self._key_type = key_type
        self._value_type = None if value_type is ... else value_type

    @property
    def old(self):
        global __symbex__
        assert self._state is not __symbex__.prev_state, "cannot use old twice!"
        return Map(self._key_type, self._value_type, __symbex__.prev_state, self._map)

    def __contains__(self, key):
        (_, present) = self._map.get(self._state, type_unwrap(key, self._map.meta.key_size))
        return present

    def __getitem__(self, key):
        (value, present) = self._map.get(self._state, type_unwrap(key, self._map.meta.key_size))
        if not present:
            raise Exception("Spec called get but element may not be there")
        return type_wrap(value, self._value_type)

    def forall(self, pred):
        return self._map.forall(self._state, lambda k, v: pred(type_wrap(k, self._key_type), type_wrap(v, self._value_type)))

    # we can't override __len__ because python enforces that it returns an 'int'
    @property
    def length(self):
        return self._map.length()


# === Time ===

# Set in spec_wrapper
time = lambda: None


# === Config ===

class _SpecConfig:
    def __init__(self, meta, devices_count):
        self._meta = meta
        self._devices_count = devices_count

    @property
    def devices_count(self):
        return self._devices_count

    def __getitem__(self, index):
        if index not in self._meta:
            raise Exception("Unknown config item: " + str(index))
        return self._meta[index]


# === Network headers ===

# TODO remove and make typed instead?
_EthernetHeader = namedtuple(
    "_EthernetHeader", [
        "dst",
        "src",
        "type"
    ]
)

_IPv4Header = namedtuple(
    "_Ipv4Header", [
        # TODO other fields - don't care for now
        "version",
        "ihl",
        "total_length",
        "time_to_live",
        "protocol",
        "checksum",
        "src",
        "dst"
    ]
)

_TcpUdpHeader = namedtuple(
    "_TcpUdpHeader", [
        "src",
        "dst"
    ]
)


# === Network devices ===

class _SpecFloodedDevice:
    def __init__(self, orig_device, devices_count):
        self._orig_device = orig_device
        self._devices_count = devices_count

    def __contains__(self, item):
        return item != self._orig_device

    @property
    def length(self):
        return self._devices_count - 1

class _SpecSingleDevice:
    def __init__(self, device):
        self._device = device

    def __contains__(self, item):
        return item == self._device

    @property
    def length(self):
        return 1


# === Network packet ===

class _SpecPacket:
    def __init__(self, data, length, devices):
        self.data = data
        self.length = length
        self._devices = devices

    @property
    def device(self):
        if isinstance(self._devices, _SpecSingleDevice):
            return self._devices._device
        raise Exception("The packet was sent on multiple devices")

    @property
    def devices(self):
        return self._devices

    @property
    def ether(self):
        return _EthernetHeader(
            dst=self.data[6*8-1:0],
            src=self.data[12*8-1:6*8],
            type=self.data[14*8-1:12*8]
        )

    @property
    def ipv4(self):
        if self.ether is None:
            return None
        if self.ether.type != 0x0008: # TODO handle endness in spec
            return None
        start = 14*8
        return _IPv4Header(
            version=self.data[start+4-1:start],
            ihl=self.data[start+8-1:start+4],
            total_length=self.data[start+4*8-1:start+2*8],
            time_to_live=self.data[start+9*8-1:start+8*8],
            protocol=self.data[start+10*8-1:start+9*8],
            checksum=self.data[start+12*8-1:start+10*8],
            src=self.data[start+16*8-1:start+12*8],
            dst=self.data[start+20*8-1:start+16*8]
        )

    @property
    def tcpudp(self):
        if self.ipv4 is None:
            return None
        if (self.ipv4.protocol != 6) & (self.ipv4.protocol != 17):
            return None

        return _TcpUdpHeader(
            src=self.data[36*8-1:34*8],
            dst=self.data[38*8-1:36*8]
        )


# === Network 'built-in' functions ===

def ipv4_checksum(header):
    return header.checksum # TODO


# === Spec wrapper ===

def _spec_wrapper(data):
    global __symbex__
    print("ze path is", __symbex__.state.path._value._segments)

    global time
    time = lambda: data.times[0] # TODO fix the whole time thing! (make it a spec arg!)

    received_packet = _SpecPacket(data.network.received, data.network.received_length, _SpecSingleDevice(data.network.received_device))
    
    transmitted_packet = None
    if len(data.network.transmitted) != 0:
        if len(data.network.transmitted) > 1:
            raise Exception("TODO support multiple transmitted packets")
        tx_dev_int = data.network.transmitted[0][2]
        if tx_dev_int is None:
            transmitted_device = _SpecFloodedDevice(data.network.received_device, data.devices_count)
        else:
            transmitted_device = _SpecSingleDevice(tx_dev_int)
        transmitted_packet = _SpecPacket(data.network.transmitted[0][0], data.network.transmitted[0][1], transmitted_device)

    config = _SpecConfig(data.config, data.devices_count)

    spec(received_packet, config, transmitted_packet)
