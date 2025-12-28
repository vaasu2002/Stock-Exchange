#!/bin/bash

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${MAGENTA}========================================${NC}"
echo -e "${MAGENTA}  Building and Running All Tests${NC}"
echo -e "${MAGENTA}========================================${NC}\n"

# Build the project
echo -e "${CYAN}[BUILD]${NC} Configuring CMake..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR]${NC} CMake configuration failed!"
    exit 1
fi

echo -e "${CYAN}[BUILD]${NC} Building project..."
cmake --build build --config Release -- -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR]${NC} Build failed!"
    exit 1
fi

echo -e "${GREEN}[BUILD]${NC} Build successful!\n"

# Test 1: IPC Crash Recovery
echo -e "${MAGENTA}========================================${NC}"
echo -e "${MAGENTA}  Running IPC Crash Recovery Tests${NC}"
echo -e "${MAGENTA}========================================${NC}\n"

./build/test_ipc_crash
IPC_RESULT=$?

echo ""

# Test 2: Gateway Network Tests
echo -e "${MAGENTA}========================================${NC}"
echo -e "${MAGENTA}  Running Gateway Network Tests${NC}"
echo -e "${MAGENTA}========================================${NC}\n"

# Start Gateway in background
echo -e "${YELLOW}[SETUP]${NC} Starting Gateway on port 9000..."
./build/Gateway > gateway.log 2>&1 &
GATEWAY_PID=$!

# Wait for gateway to start
sleep 2

# Check if gateway is running
if ! kill -0 $GATEWAY_PID 2>/dev/null; then
    echo -e "${RED}[ERROR]${NC} Gateway failed to start! Check gateway.log"
    cat gateway.log
    exit 1
fi

echo -e "${GREEN}[SETUP]${NC} Gateway started (PID: $GATEWAY_PID)\n"

# Run gateway tests
./build/test_gateway
GATEWAY_RESULT=$?

echo ""

# Cleanup
echo -e "${YELLOW}[CLEANUP]${NC} Stopping Gateway..."
kill $GATEWAY_PID 2>/dev/null
wait $GATEWAY_PID 2>/dev/null

# Summary
echo -e "${MAGENTA}========================================${NC}"
echo -e "${MAGENTA}  Test Summary${NC}"
echo -e "${MAGENTA}========================================${NC}"

if [ $IPC_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓${NC} IPC Crash Recovery Tests: PASSED"
else
    echo -e "${RED}✗${NC} IPC Crash Recovery Tests: FAILED"
fi

if [ $GATEWAY_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Gateway Network Tests: PASSED"
else
    echo -e "${RED}✗${NC} Gateway Network Tests: FAILED"
fi

echo -e "${MAGENTA}========================================${NC}\n"

# Exit with failure if any test failed
if [ $IPC_RESULT -ne 0 ] || [ $GATEWAY_RESULT -ne 0 ]; then
    echo -e "${RED}Some tests failed. Check output above.${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi