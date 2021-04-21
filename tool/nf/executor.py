import angr
import claripy
import datetime
import subprocess
import os

from binary import statistics
import binary.clock as binary_clock
import binary.executor as binary_executor
import binary.utils as utils
import binary.ghost_maps as ghost_maps
from binary.externals.os import clock
from binary.externals.os import config
from binary.externals.os import log
from binary.externals.os import memory
from binary.externals.os import pci
from binary.externals.net import packet
from binary.externals.net import tx
from binary.externals.structs import map
from binary.externals.structs import map2
from binary.externals.structs import index_pool
from binary.externals.structs import cht
from binary.externals.structs import lpm
from binary.externals.verif import verif
from . import spec_act
from . import spec_reg

# TODO: all these externals should be declared in nf... not in binary... !

structs_alloc_externals = {
    'map_alloc': map.map_alloc,
    'os_map2_alloc': map2.OsMap2Alloc,
    'index_pool_alloc': index_pool.index_pool_alloc,
    'cht_alloc': cht.ChtAlloc,
    'lpm_alloc': lpm.LpmAlloc,
}

structs_functions_externals = {
    'map_get': map.map_get,
    'map_set': map.map_set,
    'map_remove': map.map_remove,
    'os_map2_get': map2.OsMap2Get,
    'os_map2_set': map2.OsMap2Set,
    'os_map2_remove': map2.OsMap2Remove,
    'index_pool_borrow': index_pool.index_pool_borrow,
    'index_pool_return': index_pool.index_pool_return,
    'index_pool_refresh': index_pool.index_pool_refresh,
    'index_pool_used': index_pool.index_pool_used,
    'cht_find_preferred_available_backend': cht.ChtFindPreferredAvailableBackend,
    'lpm_lookup_elem': lpm.LpmLookupElem,
}


def find_fixedpoint_states(states_data):
    inference_results = None
    while True:
        print("Running an iteration of the main loop at", datetime.datetime.now())
        statistics.work_start("symbex")
        result_states = []
        for (state, state_fun) in states_data:
            starting_state = state_fun(state.copy())
            result_states += binary_executor.run_state(starting_state)
        statistics.work_end()
        print("Inferring invariants on", len(result_states), "states at", datetime.datetime.now())
        states = [s for (s, _) in states_data]
        statistics.work_start("infer")
        (states, inference_results, reached_fixpoint) = ghost_maps.infer_invariants(states, result_states, inference_results)
        statistics.work_end()
        if reached_fixpoint:
            return result_states
        states_data = [(new_s, fun) for (new_s, (old_s, fun)) in zip(states, states_data)]


# === libNF ===

libnf_init_externals = {
    'os_config_try_get': config.os_config_try_get,
    'os_memory_alloc': memory.os_memory_alloc
}
libnf_init_externals.update(structs_alloc_externals)

libnf_handle_externals = {
    'os_debug': log.os_debug,
    'net_transmit': tx.net_transmit,
    'net_flood': tx.net_flood,
    'net_flood_except': tx.net_flood_except
}
libnf_handle_externals.update(structs_functions_externals)

# subprocess.check_call(["make", "-f" "../Makefile.nf"], cwd=nf_folder) TODO also for full-stack
def get_libnf_inited_states(binary_path, devices_count):
    blank_state = binary_executor.create_blank_state(binary_path)
    # If needed, hook cpu_freq symbols (TODO remove this when we rework time stuff!)
    cpu_freq_numerator = blank_state.project.loader.find_symbol("cpu_freq_numerator")
    if cpu_freq_numerator is None:
        print("Warning: No cpu freq symbols detected, skipping...")
    else:
        cpu_freq_denominator = blank_state.project.loader.find_symbol("cpu_freq_denominator")
        blank_state.memory.store(cpu_freq_numerator.rebased_addr, binary_clock.frequency_num, endness=blank_state.arch.memory_endness)
        blank_state.memory.store(cpu_freq_denominator.rebased_addr, binary_clock.frequency_denom, endness=blank_state.arch.memory_endness)
    # Create and run an init state
    # TODO Something very fishy in here, why do we need to reverse the arg? angr's endianness handling keeps puzzling me
    init_state = binary_executor.create_calling_state(blank_state, "nf_init", [devices_count.reversed], libnf_init_externals)
    init_state.solver.add(devices_count.UGT(0))
    result_states = binary_executor.run_state(init_state)
    # Create handle states from all successful inits
    inited_states = []
    for state in result_states:
        # code to get the return value copied from angr's "Callable" implementation
        cc = angr.DEFAULT_CC[state.project.arch.name](state.project.arch)
        init_result = cc.get_return_val(state, stack_base=state.regs.sp - cc.STACKARG_SP_DIFF)
        state.solver.add(init_result != 0)
        if state.solver.satisfiable():
            state_creator = lambda st: binary_executor.create_calling_state(st, "nf_handle", [packet.alloc(st, devices_count)], libnf_handle_externals)
            inited_states.append((state, state_creator))
    return inited_states

def execute_libnf(binary_path):
    print("libNF symbex starting at", datetime.datetime.now())
    statistics.work_start("symbex")
    devices_count = claripy.BVS('devices_count', 16) # TODO avoid the hardcoded 16 here
    inited_states = get_libnf_inited_states(binary_path, devices_count)
    statistics.work_end()
    result_states = find_fixedpoint_states(inited_states)
    print("libNF symbex done at", datetime.datetime.now())
    return (result_states, devices_count) # TODO devices_count should be in metadata somewhere, not explicitly returned


# === Full-stack ===

nf_init_externals = {
    'os_clock_sleep_ns': clock.os_clock_sleep_ns,
    'os_config_try_get': config.os_config_try_get,
    'os_memory_alloc': memory.os_memory_alloc,
    'os_memory_phys_to_virt': memory.os_memory_phys_to_virt,
    'os_memory_virt_to_phys': memory.os_memory_virt_to_phys,
    'os_pci_enumerate': pci.os_pci_enumerate,
    'descriptor_ring_alloc': verif.descriptor_ring_alloc,
    'agents_alloc': verif.agents_alloc,
    'foreach_index_forever': verif.foreach_index_forever
}
nf_init_externals.update(structs_alloc_externals)

nf_handle_externals = structs_functions_externals

nf_inited_states = [] # "global" for use in externals/verif/verif.py
def execute_nf(binary_path):
    print("NF symbex starting at", datetime.datetime.now())
    spec_reg.validate_registers(spec_reg.registers)
    spec_reg.validate_registers(spec_reg.pci_regs)
    spec_act.validate_actions()
    blank_state = binary_executor.create_blank_state(binary_path)
    init_state = binary_executor.create_calling_state(blank_state, "_start", [], nf_init_externals)
    global nf_inited_states
    assert nf_inited_states is not None
    nf_inited_states = []
    binary_executor.run_state(init_state, allow_trap=True) # this will fill nf_inited_states; we allow traps only here since that's how init can fail
    assert len(nf_inited_states) > 0
    result_states = find_fixedpoint_states(nf_inited_states)
    print("NF symbex done at", datetime.datetime.now())
    return (result_states, claripy.BVV(2, 16)) # TODO ouch hardcoding, same remark as in execute_libnf
