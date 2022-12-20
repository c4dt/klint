#!/bin/sh -eu

# Input: Files to compile
# Optional extra input: environment variable $EXTRA_BPF_CFLAGS for the BPF compilation
# Output, as files in the working directory:
# - 'bpf.obj', the compiled BPF bytecode
# - 'bpf.bin', the kernel-JITed native code
# - 'bpf.calls', a file with one line per called BPF helper, format '[hex kernel address] [name]'
# - 'bpf.maps', a file with one line per BPF map, format '[hex kernel address] [hex data]'

# TODO split needed?
readonly BPFTOOL=${BPFTOOL:=bpftool}
readonly LINUX_BPFTOOL=${LINUX_BPFTOOL:=$BPFTOOL}

readonly LIBBPF_CFLAGS=${LIBBPF_CFLAGS:=$(pkg-config libbpf --cflags)}

if ! type "$BPFTOOL" > /dev/null
then
  echo "$BPFTOOL not found, please install it"
  exit 1
fi
if ! "$BPFTOOL" version | grep -qE 'features: .*\<libbfd\>'
then
  echo "$BPFTOOL found, but doesn't feature libbfd"
  exit 1
fi

if ! type clang > /dev/null
then
  echo 'clang not found, please install it.'
  exit 1
fi

# Compile
clang -O3 -target bpf \
	$LIBBPF_CFLAGS \
	-D u8=__u8 -D u16=__u16 -D u32=__u32 -D u64=__u64 -D __wsum=__u32 -D __sum16=__u16 \
	-o bpf.obj -c $@
	# TODO $EXTRA_BPF_CFLAGS

# Ensure kernel BPF JIT is enabled
echo 1 | sudo tee '/proc/sys/net/core/bpf_jit_enable' >/dev/null

# Remove an existing program, just in case some previous script run failed
sudo rm -f '/sys/fs/bpf/temp'

# Load into kernel
sudo "$BPFTOOL" prog load 'bpf.obj' '/sys/fs/bpf/temp'

# Dump BPF as text
sudo "$LINUX_BPFTOOL" prog dump xlated pinned '/sys/fs/bpf/temp' > '/tmp/bpf'

# Dump x86 as text
sudo "$LINUX_BPFTOOL" prog dump jited pinned '/sys/fs/bpf/temp' > '/tmp/x86'

# Create the calls list (address to name)
grep -F call '/tmp/x86' | sed 's/.*\(0x.*\)/\1/' > '/tmp/x86-calls'
grep -F call '/tmp/bpf' | sed 's/.*call \(.*\)#.*/\1/' > '/tmp/bpf-calls'
paste -d ' ' '/tmp/x86-calls' '/tmp/bpf-calls' | sort | uniq > 'bpf.calls'

# Create the maps list (address to maps)
# First, create a mapping from addresses to names, using the order in which loads and relocations appear
sed 's/.*movabs $\(0x[0-9a-z]\{16\}\),%rdi/\1/;t;d' '/tmp/x86' > '/tmp/map-addrs'
objdump -r 'bpf.obj' | tail -n+6 | tr -s ' ' | cut -d ' ' -f 3 | grep -Fv '.bss' | head -n-2 > '/tmp/map-names'
paste -d ' ' '/tmp/map-addrs' '/tmp/map-names' | sort | uniq > '/tmp/addrs-to-names'
# Then, create a mapping from names to data
MAPS_SECTION_IDX="$(readelf --sections 'bpf.obj' | sed 's/.*\[\s*\([0-9]*\)\]\s*maps.*/\1/;t;d')"
objdump -h 'bpf.obj' | grep '.maps' | head -n 1 | awk '{print "dd if='bpf.obj' of='/tmp/maps' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}' | bash 2>/dev/null # From https://stackoverflow.com/a/3925586
readelf -s 'bpf.obj' | tail -n+4 | tr -s ' ' | grep -F "DEFAULT $MAPS_SECTION_IDX" | cut -d ' ' -f 3,9 > '/tmp/offset-to-name'
cat '/tmp/offset-to-name' | awk '{print "echo -n " $2 " ; echo -n '"' '"' ; xxd -p -seek 0x" $1 " -l 20 /tmp/maps"}' | sh > '/tmp/names-to-contents'
# Finally, combine the two
cat '/tmp/addrs-to-names' | awk '{ print "echo " $1 " $(grep \"^" $2 " \" /tmp/names-to-contents)"  }' | sh > 'bpf.maps'

# Dump x86 as binary
sudo "$LINUX_BPFTOOL" prog dump jited pinned '/sys/fs/bpf/temp' file '/tmp/bin'
sudo chmod 644 '/tmp/bin'
cp '/tmp/bin' 'bpf.bin'

# Remove
sudo rm '/sys/fs/bpf/temp'
