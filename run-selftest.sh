#!/bin/bash

# KnishIO C SDK Self-Test Build and Run Script
# 
# This script builds the C SDK self-test executable and runs it,
# following the same pattern as other SDK self-test scripts.

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔨 Building KnishIO C SDK Self-Test...${NC}"

# Clean CMake cache to avoid absolute path issues
echo -e "${YELLOW}🧹 Cleaning CMake cache for fresh build...${NC}"
rm -f CMakeCache.txt
rm -rf CMakeFiles/
rm -rf cmake-build-*/
rm -rf _deps/  # Clean FetchContent dependencies cache
rm -rf build/  # Clean build directory completely

# Create fresh build directory
echo -e "${YELLOW}📁 Creating clean build directory...${NC}"
mkdir -p build

# Configure with CMake (build in source directory)
echo -e "${YELLOW}⚙️  Configuring CMake build...${NC}"
cmake . \
    -DCMAKE_BUILD_TYPE=Release \
    -DKNISHIO_BUILD_TESTS=ON \
    -DKNISHIO_BUILD_EXAMPLES=ON \
    -DKNISHIO_ENABLE_SANITIZERS=OFF

# Build the working self-test target (complete JavaScript parity)
echo -e "${YELLOW}🔧 Building c_self_test target...${NC}"
make c_self_test

# Check if build was successful
if [ ! -f "c_self_test" ]; then
    echo -e "${RED}❌ Build failed - c_self_test executable not found${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Build completed successfully${NC}"
echo -e "${BLUE}🚀 Running C SDK Self-Test (Full JavaScript Parity)...${NC}"
echo ""

# Run the self-test
./c_self_test
TEST_EXIT_CODE=$?

# Report results
echo ""
if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}🎉 C SDK Self-Test completed successfully!${NC}"
else
    echo -e "${RED}💥 C SDK Self-Test failed with exit code $TEST_EXIT_CODE${NC}"
fi

# Return to original directory
cd ..

exit $TEST_EXIT_CODE