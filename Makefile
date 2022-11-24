# Get current dir, see https://stackoverflow.com/a/8080530
ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

OS ?= linux
NET ?= tinynf
OS_CONFIG ?= $(ROOT_DIR)/env/config

## others

.DEFAULT_GOAL := compile-all

include $(ROOT_DIR)/base.mk
include $(ROOT_DIR)/nf/nf.mk

.PHONY: compile-all
compile-all: $(addprefix compile-,$(NFs))

build-%: compile-%
	@if [ ! -f $(OS_CONFIG) ]; then echo 'Please write an OS config file in $(OS_CONFIG), see $(ROOT_DIR)/env/ReadMe.md'; exit 1; fi
	$(MAKE) -C $(ROOT_DIR)/env NF=$(ROOT_DIR)/nf/$*/libnf.so OS=$(OS) NET=$(NET) OS_CONFIG=$(OS_CONFIG) NF_CONFIG=$(ROOT_DIR)/nf/$*/config

include $(ROOT_DIR)/tool/tool.mk
verify-%: compile-% | tool-venv
	. $(TOOL_VENV_DIR)/bin/activate && \
		klint libnf $(ROOT_DIR)/nf/$*/libnf.so $(ROOT_DIR)/nf/$*/spec.py

benchmark-%: compile-%
	@if [ ! -f $(ROOT_DIR)/benchmarking/config ]; then echo 'Please set the benchmarking config, see $(ROOT_DIR)/benchmarking/ReadMe.md'; exit 1; fi
	@if [ '$(NF_LAYER)' = '' ]; then echo 'Please set NF_LAYER to the layer of your NF, e.g., 2 for a bridge, 4 for a TCP/UDP firewall'; exit 1; fi
	NF=$(ROOT_DIR)/nf/$* OS=$(OS) NET=$(NET) $(ROOT_DIR)/benchmarking/bench.sh '$(ROOT_DIR)/env' standard $(NF_LAYER)
