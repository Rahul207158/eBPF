# Makefile for eBPF XDP Packet Filter

CC := gcc
CLANG := clang
ARCH := $(shell uname -m | sed 's/x86_64/x86/')

# Directories
SRC_DIR := .
BUILD_DIR := build

# Source files
BPF_SRC := packet_filter.bpf.c
USER_SRC := packet_filter.c
BPF_OBJ := packet_filter.bpf.o
USER_BIN := packet_filter

# Compiler flags
CFLAGS := -O2 -g -Wall -Wextra
BPF_CFLAGS := -O2 -g -Wall -Wextra -target bpf -D__TARGET_ARCH_$(ARCH)
USER_CFLAGS := $(CFLAGS) -I/usr/include/bpf
USER_LDFLAGS := -lbpf -lelf -lz

# Include directories
INCLUDES := -I/usr/include/$(shell uname -m)-linux-gnu

# Default target
all: $(BPF_OBJ) $(USER_BIN)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile eBPF program
$(BPF_OBJ): $(BPF_SRC)
	$(CLANG) $(BPF_CFLAGS) $(INCLUDES) -c $< -o $@

# Compile userspace program
$(USER_BIN): $(USER_SRC) $(BPF_OBJ)
	$(CC) $(USER_CFLAGS) $(INCLUDES) $< -o $@ $(USER_LDFLAGS)

# Clean build artifacts
clean:
	rm -f $(BPF_OBJ) $(USER_BIN)
	rm -rf $(BUILD_DIR)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt update
	sudo apt install -y \
		clang \
		llvm \
		gcc \
		libbpf-dev \
		libelf-dev \
		linux-headers-$(shell uname -r) \
		build-essential

# Install dependencies (RHEL/CentOS/Fedora)
install-deps-rhel:
	sudo dnf install -y \
		clang \
		llvm \
		gcc \
		libbpf-devel \
		elfutils-libelf-devel \
		kernel-headers \
		kernel-devel

# Load program (example)
load: $(USER_BIN)
	@echo "Loading XDP packet filter..."
	@echo "Usage: sudo ./$(USER_BIN) -i <interface> [-p <port>] [-s]"
	@echo "Example: sudo ./$(USER_BIN) -i eth0 -p 4040 -s"

# Test with default settings
test: $(USER_BIN)
	@echo "Testing with default port 4040 on eth0..."
	@echo "Make sure to run as root!"
	sudo ./$(USER_BIN) -i eth0 -p 4040 -s

# Show kernel messages
show-logs:
	sudo dmesg | tail -20

# Show trace pipe (for bpf_printk output)
show-trace:
	sudo cat /sys/kernel/debug/tracing/trace_pipe

# Check if BPF filesystem is mounted
check-bpf:
	@if [ ! -d /sys/fs/bpf ]; then \
		echo "BPF filesystem not mounted. Mounting..."; \
		sudo mount -t bpf bpf /sys/fs/bpf/; \
	else \
		echo "BPF filesystem is mounted"; \
	fi

# List network interfaces
list-interfaces:
	@echo "Available network interfaces:"
	@ip link show | grep '^[0-9]' | cut -d':' -f2 | sed 's/^ *//'

# Help target
help:
	@echo "Available targets:"
	@echo "  all                - Build both eBPF and userspace programs"
	@echo "  clean              - Clean build artifacts"
	@echo "  install-deps       - Install dependencies (Ubuntu/Debian)"
	@echo "  install-deps-rhel  - Install dependencies (RHEL/CentOS/Fedora)"
	@echo "  load               - Show usage information"
	@echo "  test               - Test with default settings"
	@echo "  show-logs          - Show kernel messages"
	@echo "  show-trace         - Show BPF trace output"
	@echo "  check-bpf          - Check/mount BPF filesystem"
	@echo "  list-interfaces    - List available network interfaces"
	@echo "  help               - Show this help message"

.PHONY: all clean install-deps install-deps-rhel load test show-logs show-trace check-bpf list-interfaces help 