from binary.memory_fractional import RecordAllocateOpaque
from binary.ghost_maps import *
from binary import utils

class ReplayException(Exception): pass

def expect(state, expected):
    while True:
        actual = state.path.ghost_dequeue()
        if type(expected) == type(actual):
            ed = expected._asdict()
            ad = actual._asdict()
            if all(ed[k] is None or str(ed[k]) == str(ad[k]) for k in ed.keys()):
                return actual
        if not isinstance(actual, RecordGet):
            raise ReplayException(f"Replay: expected {expected} but got {actual}")


class SymbolFactoryReplayPlugin:
    def __init__(self, history):
        self.history = history
        self.index = 0

    def dequeue(self):
        result = self.history[self.index]
        self.index = self.index + 1
        return result

    def BVS(self, name, size):
        (actual_name, result) = self.dequeue()
        if actual_name == name:
            return result
        raise ReplayException(f"BVS replay: expected {name} but got {actual_name}")
    
class MemoryAllocateOpaqueReplayPlugin:
    def __init__(self, wrapped):
        self.wrapped = wrapped

    def __getattr__(self, name):
        return getattr(self.wrapped, name)

    def allocate_opaque(self, name):
        return expect(self.wrapped.state, RecordAllocateOpaque(name, None)).result

class GhostMapsReplayPlugin:
    def __init__(self, state):
        self.state = state
        self.metas = {}

    def new(self, key_size, value_size, name="map"):
        obj = self.state.memory.allocate_opaque(name)
        expect(self.state, RecordNew(key_size, value_size, obj))
        self.metas[obj] = MapMeta(name, key_size, value_size)
        return obj

    def new_array(self, key_size, value_size, length, name="map"):
        obj = self.state.memory.allocate_opaque(name)
        expect(self.state, RecordNewArray(key_size, value_size, length, obj))
        self.metas[obj] = MapMeta(name, key_size, value_size)
        return obj

    def length(self, obj):
        return expect(self.state, RecordLength(obj, None)).result

    def key_size(self, obj):
        return self.metas[obj].key_size

    def value_size(self, obj):
        return self.metas[obj].value_size

    def get(self, obj, key, value=None, from_present=True):
        return expect(self.state, RecordGet(obj, key, None)).result

    def set(self, obj, key, value):
        expect(self.state, RecordSet(obj, key, value))

    def remove(self, obj, key):
        expect(self.state, RecordRemove(obj, key))

    def forall(self, obj, pred):
        record = expect(self.state, RecordForall(obj, None, None, None, None))
        if str(pred(record.pred_key, record.pred_value)) == str(record.pred): # idk why but structurally_match seems to fail for no reason
            return record.result
        raise ReplayException(f"Forall replay: expected {record.pred} but got {pred(record.pred_key, record.pred_value)}")

    def havoc(self, obj, max_length, is_array):
        pass # ignore for now...