import angr
import claripy

from binary import executor as binary_executor
from nf import executor as nf_executor


def _custom_memory(memory_count, memory_size): # size in bits
    memory = {}
    def reader(state, base, index, offset, size):
        if size is None:
            size = memory_size
        else:
            size = size * 8
        assert not index.symbolic
        index = state.solver.eval(index)
        assert index < memory_count
        return memory.setdefault(index, claripy.BVV(0, memory_size))[size-1:offset]

    def writer(state, base, index, offset, value):
        assert not index.symbolic
        index = state.solver.eval(index)
        assert index < memory_count
        existing_value = memory.setdefault(index, claripy.BVV(0, memory_size))
        if offset != 0:
            value = value.concat(existing_value[offset-1:0])
        if value.size() != memory_size:
            value = existing_value[:value.size()].concat(value)
        memory[index] = value
    return (reader, writer)



# uint64_t* descriptor_ring_alloc(size_t count);
class descriptor_ring_alloc(angr.SimProcedure):
    def run(self, count):
        count = self.state.casts.size_t(count)
        (ring_reader, ring_writer) = _custom_memory(count, self.state.sizes.uint64_t * 2)
        return self.state.memory.create_special_object("descriptor_ring", count, (self.state.sizes.uint64_t * 2) // 8, ring_reader, ring_writer)

# void* agents_alloc(size_t count, size_t size);
class agents_alloc(angr.SimProcedure):
    def run(self, count, size):
        count = self.state.casts.size_t(count)
        size = self.state.casts.size_t(size)
        (reader, writer) = _custom_memory(count, size * 8)
        return self.state.memory.create_special_object("agents", count, size, reader, writer)

# typedef void foreach_index_forever_function(size_t index, void* state);
# _Noreturn void foreach_index_forever(size_t length, foreach_index_forever_function* func, void* state);
class foreach_index_forever(angr.SimProcedure):
    NO_RET = True # magic angr variable

    def run(self, length, func, st):
        length = self.state.casts.size_t(length)
        func = self.state.casts.ptr(func)
        st = self.state.casts.ptr(st)

        if func.op != 'BVV':
            raise Exception("Function pointer cannot be symbolic")
        self.state.solver.add(length.UGT(0))

        # Ideally:
        #index = claripy.BVS("foreach_index", self.state.sizes.size_t)
        #self.state.solver.add(index.ULT(length))
        # But:
        concrete_length = utils.get_if_constant(self.state.solver, length)
        assert length is not None
        for idx in range(concrete_length):
            index = claripy.BVV(idx, self.state.sizes.size_t)
            func_state = binary_executor.create_calling_state(self.state, func, [index, st], nf_executor.nf_handle_externals)
            nf_executor.nf_inited_states.append(func_state)

        self.exit(0)