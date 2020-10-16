from angr.state_plugins.sim_action import SimActionObject
import binary.bitsizes as bitsizes

# TODO make this arch-dependent (maybe as a state plugin?)

def _fix(o):
    if isinstance(o, SimActionObject):
        o = o.ast
    return o

def uint64_t(o):
    return _fix(o)

def uint32_t(o):
    return _fix(o)[31:0]

def uint16_t(o):
    return _fix(o)[15:0]

def int64_t(o):
    return _fix(o)

def ptr(o):
    return _fix(o)

def size_t(o):
    return uint64_t(o)