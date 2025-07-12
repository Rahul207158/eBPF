#!/bin/bash

# eBPF XDP Packet Filter Demo Script
# This script demonstrates the packet filtering functionality

set -e

INTERFACE="lo"  # Use loopback interface for testing
PORT="4040"     # Default port to filter
TEST_PORT="8080" # Port that should pass through

echo "=== eBPF XDP Packet Filter Demo ==="
echo "This demo will show how TCP packets are dropped on port $PORT"
echo "while allowing packets on port $TEST_PORT to pass through."
echo

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root (use sudo)"
   exit 1
fi

# Check if program is built
if [[ ! -f "./packet_filter" ]]; then
    echo "Building the packet filter program..."
    make clean
    make
fi

# Function to cleanup background processes
cleanup() {
    echo "Cleaning up..."
    pkill -f "packet_filter" 2>/dev/null || true
    pkill -f "python3 -m http.server" 2>/dev/null || true
    pkill -f "nc -l" 2>/dev/null || true
    sleep 2
}

# Set up cleanup on exit
trap cleanup EXIT

echo "Step 1: Starting packet filter on interface $INTERFACE, port $PORT"
echo "Command: ./packet_filter -i $INTERFACE -p $PORT -s"
echo

# Start packet filter in background
./packet_filter -i $INTERFACE -p $PORT -s &
FILTER_PID=$!

# Wait for filter to start
sleep 3

echo "Step 2: Starting test servers"
echo "- HTTP server on port $PORT (should be blocked)"
echo "- HTTP server on port $TEST_PORT (should work)"
echo

# Start HTTP servers in background
python3 -m http.server $PORT &
SERVER1_PID=$!

python3 -m http.server $TEST_PORT &
SERVER2_PID=$!

# Wait for servers to start
sleep 3

echo "Step 3: Testing connectivity"
echo

# Test blocked port
echo "Testing blocked port $PORT:"
echo "Command: curl -m 5 http://localhost:$PORT"
if curl -m 5 http://localhost:$PORT 2>/dev/null; then
    echo "❌ ERROR: Port $PORT should be blocked but is accessible!"
else
    echo "✅ SUCCESS: Port $PORT is blocked as expected"
fi
echo

# Test allowed port
echo "Testing allowed port $TEST_PORT:"
echo "Command: curl -m 5 http://localhost:$TEST_PORT"
if curl -m 5 http://localhost:$TEST_PORT >/dev/null 2>&1; then
    echo "✅ SUCCESS: Port $TEST_PORT is accessible as expected"
else
    echo "❌ ERROR: Port $TEST_PORT should be accessible but is blocked!"
fi
echo

echo "Step 4: Generating more test traffic"
echo "Sending multiple requests to demonstrate packet filtering..."

# Generate test traffic
for i in {1..10}; do
    curl -m 2 http://localhost:$PORT >/dev/null 2>&1 || true
    curl -m 2 http://localhost:$TEST_PORT >/dev/null 2>&1 || true
done

echo "Test traffic generated."
echo

echo "Step 5: Final statistics"
echo "The packet filter should show statistics of dropped vs passed packets."
echo "Let it run for a few more seconds to collect final stats..."

sleep 5

echo
echo "=== Demo Complete ==="
echo "Key observations:"
echo "- TCP packets to port $PORT were dropped by the XDP filter"
echo "- TCP packets to port $TEST_PORT passed through normally"
echo "- The filter operates at the kernel level with minimal overhead"
echo "- Statistics show the effectiveness of the filtering"
echo
echo "Press Ctrl+C to stop the packet filter and exit."

# Keep the filter running until user stops it
wait $FILTER_PID 