# Quick Start Guide - eBPF XDP TCP Packet Filter

Get up and running with the eBPF XDP TCP packet filter in just a few minutes!

## TL;DR - Super Quick Start

```bash
# 1. Install dependencies (Ubuntu/Debian)
sudo apt update && sudo apt install -y clang llvm gcc libbpf-dev libelf-dev linux-headers-$(uname -r) build-essential

# 2. Build the project
make

# 3. Run the demo (uses loopback interface - safe for testing)
sudo ./test_demo.sh

# 4. Or run manually on a specific interface
sudo ./packet_filter -i eth0 -p 4040 -s
```

## What This Does

- **Drops TCP packets** on port 4040 (or any port you specify)
- **Configurable from userspace** - no need to recompile
- **High performance** - up to 24 million packets per second
- **Real-time statistics** - see what's being filtered

## Prerequisites Check

```bash
# Check kernel version (need 4.8+)
uname -r

# Check if you have root access
sudo echo "Root access: OK"

# List available network interfaces
ip link show
```

## Installation

### Option 1: Auto-install dependencies

```bash
# Ubuntu/Debian
make install-deps

# RHEL/CentOS/Fedora
make install-deps-rhel
```

### Option 2: Manual installation

**Ubuntu/Debian:**

```bash
sudo apt update
sudo apt install -y clang llvm gcc libbpf-dev libelf-dev linux-headers-$(uname -r) build-essential
```

**RHEL/CentOS/Fedora:**

```bash
sudo dnf install -y clang llvm gcc libbpf-devel elfutils-libelf-devel kernel-headers kernel-devel
```

## Build

```bash
make
```

You should see:

- `packet_filter.bpf.o` - The eBPF program
- `packet_filter` - The control application

## Basic Usage

### 1. Safe Testing (Recommended First)

```bash
# Run the automated demo - uses loopback interface
sudo ./test_demo.sh
```

### 2. Manual Testing

```bash
# Drop TCP packets on port 4040 on eth0
sudo ./packet_filter -i eth0 -p 4040 -s

# In another terminal, test it:
# This should be blocked:
curl http://your-server-ip:4040

# This should work:
curl http://your-server-ip:8080
```

### 3. Common Use Cases

```bash
# Block SSH access
sudo ./packet_filter -i eth0 -p 22 -s

# Block HTTP
sudo ./packet_filter -i eth0 -p 80 -s

# Block HTTPS
sudo ./packet_filter -i eth0 -p 443 -s

# Block custom application port
sudo ./packet_filter -i eth0 -p 8080 -s
```

## Monitoring

### View Statistics

When running with `-s`, you'll see:

```
=== Packet Statistics ===
Total packets:   15420
TCP packets:     8932
Dropped packets: 145
Passed packets:  15275
Drop rate:       0.94%
========================
```

### View Debug Messages

```bash
# In another terminal while the filter is running:
sudo cat /sys/kernel/debug/tracing/trace_pipe
```

You'll see messages like:

```
packet_filter-1234  [000] d.s.1  1234.567890: bpf_trace_printk: Dropping TCP packet to port 4040
```

## Stopping the Filter

Simply press `Ctrl+C` in the terminal where the filter is running. It will:

- Detach from the network interface
- Clean up resources
- Show final statistics

## Troubleshooting

### "Permission denied"

```bash
# Make sure you're running as root
sudo ./packet_filter -i eth0 -p 4040
```

### "Interface not found"

```bash
# Check available interfaces
ip link show
make list-interfaces
```

### "Failed to load BPF program"

```bash
# Check if BPF filesystem is mounted
ls /sys/fs/bpf/

# If not, mount it:
sudo mount -t bpf bpf /sys/fs/bpf/
```

### "Compilation failed"

```bash
# Install missing dependencies
make install-deps  # Ubuntu/Debian
make install-deps-rhel  # RHEL/CentOS/Fedora
```

## Command Reference

```bash
# Basic syntax
sudo ./packet_filter -i <interface> [-p <port>] [-s]

# Options:
-i, --interface <name>    Network interface (required)
-p, --port <port>         Port to filter (default: 4040)
-s, --stats               Show statistics every 5 seconds
-h, --help                Show help

# Examples:
sudo ./packet_filter -i eth0                    # Default port 4040
sudo ./packet_filter -i eth0 -p 8080           # Custom port
sudo ./packet_filter -i eth0 -p 4040 -s        # With statistics
sudo ./packet_filter -i wlan0 -p 22 -s         # Block SSH on WiFi
```

## Testing Your Setup

### Method 1: HTTP Server Test

```bash
# Terminal 1: Start the filter
sudo ./packet_filter -i lo -p 4040 -s

# Terminal 2: Start HTTP server on filtered port
python3 -m http.server 4040

# Terminal 3: Test (should fail)
curl http://localhost:4040

# Terminal 3: Test unfiltered port (should work)
python3 -m http.server 8080  # In Terminal 2
curl http://localhost:8080   # In Terminal 3
```

### Method 2: Netcat Test

```bash
# Terminal 1: Start the filter
sudo ./packet_filter -i lo -p 4040 -s

# Terminal 2: Listen on filtered port
nc -l 4040

# Terminal 3: Try to connect (should fail)
nc localhost 4040
```

## Next Steps

- Read the full [README.md](README.md) for detailed documentation
- Check out [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md) for technical details
- Explore the source code to understand how it works
- Try modifying the port or interface
- Monitor the statistics to see the filtering in action

## Help

```bash
# Show help
./packet_filter -h

# Show Makefile targets
make help

# List network interfaces
make list-interfaces

# Show kernel messages
make show-logs

# Show BPF trace output
make show-trace
```

## Safety Notes

- Always test on a non-production system first
- Use the loopback interface (`lo`) for safe testing
- The filter affects all traffic on the specified interface
- Use `Ctrl+C` to stop - don't kill the process abruptly
- Monitor your system to ensure it's working as expected

Happy filtering! 🚀
