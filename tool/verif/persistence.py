from collections import namedtuple
import pickle

from binary.externals.os import config as os_config
from binary.externals.net import packet

StateData = namedtuple('StateData', ['maps', 'path', 'constraints', 'network', 'config', 'devices_count'])

def dump_data(states, devices_count, path): # TODO why do we have to move the devices_count around like that? :/
    data = [StateData(
        maps = {k: v.flatten(keep_known_items=True) for (k, v) in state.maps.get_all().items()},
        path = state.path,
        constraints = state.solver.constraints,
        network = state.metadata.get_one(packet.NetworkMetadata),
        config = (state.metadata.get_one(os_config.ConfigMetadata) or os_config.ConfigMetadata([])).items,
        devices_count = devices_count
    ) for state in states]
    with open(path, "wb") as file:
        pickle.dump(data, file, protocol=4)

def load_data(path):
    with open(path, "rb") as file:
        return pickle.load(file)