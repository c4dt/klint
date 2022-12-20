# existing NF
NFs := $(patsubst $(ROOT_DIR)/nf/%,%,$(shell find $(ROOT_DIR)/nf $(ROOT_DIR)/nf/bpf -mindepth 1 -maxdepth 1 -type d -not -name bpf))

# our own NF-specific cflags
NF_CFLAGS = $(CFLAGS)

# No libC, thus no extra stuff like __cxa_finalize
NF_CFLAGS += -nostdlib

# Allow only freestanding headers, nothing else (hacky but no other way to do it apparently... https://stackoverflow.com/q/2681304)
NF_CFLAGS += -ffreestanding -nostdinc -isystem $(shell gcc -print-search-dirs | head -n 1 | cut -d ':' -f 2)/include

# OS headers
NF_CFLAGS += -I$(ROOT_DIR)/env/include

# all compiles w/ libbpf
NF_BPFLAGS := $(shell pkg-config libbpf --cflags)

# add host headers for Ubuntu
ifeq ($(shell lsb_release -si),Ubuntu)
NF_BPFLAGS += -I/usr/include/$(shell arch)-linux-gnu
endif

# avoid underscoring every basic types
NF_BPFLAGS += -D u8=__u8 -D u16=__u16 -D u32=__u32 -D u64=__u64
NF_BPFLAGS += -D __wsum=__u32 -D __sum16=__u16

# targets

nf/%/libnf.so: CFLAGS := $(NF_CFLAGS)
nf/%/libnf.so: nf/%/impl.c
	$(CC) $(CFLAGS) -c $^ -o $@

nf/bpf/%/libnf.so: BPFFLAGS := $(NF_BPFLAGS)
nf/bpf/%/libnf.so: nf/bpf/%/impl.c
	 clang -target bpf $(BPFFLAGS) -I$(dir $<)include -c $^ -o $@

CARGO_TARGET := $(shell cargo -Vv | awk '/^host:/ {print $$2}')
nf/rust-policer/libnf.so: nf/rust-policer/target/$(CARGO_TARGET)/release/librust_policer.so
	cp $^ $@
nf/rust-policer/target/$(CARGO_TARGET)/release/librust_policer.so: $(wildcard nf/rust-policer/src/*.rs)
	cd nf/rust-policer && \
		cargo build --release \
			-Z build-std=std,panic_abort \
			-Z build-std-features=panic_immediate_abort \
			--target $(CARGO_TARGET)

.PHONY: $(addprefix compile-,$(NFs))
$(addprefix compile-,$(NFs)): compile-%: nf/%/libnf.so
.PHONY: compile-all
compile-all: $(foreach NF,$(NFs),nf/$(NF)/libnf.so)
