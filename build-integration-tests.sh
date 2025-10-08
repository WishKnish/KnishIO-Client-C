#!/bin/bash

# Knish.IO C SDK Integration Test Builder
# Compiles and manages C SDK integration tests with proper dependencies

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}═══════════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  KnishIO C SDK Integration Test Builder${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════════════${NC}"
echo

# Check system dependencies
echo -e "${BLUE}Checking system dependencies...${NC}"

# Check if pkg-config is available
if ! command -v pkg-config &> /dev/null; then
    echo -e "${RED}❌ pkg-config not found. Please install pkg-config.${NC}"
    exit 1
fi

# Check for required libraries
MISSING_LIBS=""

if ! pkg-config --exists libcurl; then
    MISSING_LIBS="${MISSING_LIBS}libcurl "
fi

if ! pkg-config --exists libcjson && ! pkg-config --exists cjson; then
    MISSING_LIBS="${MISSING_LIBS}libcjson "
fi

if [ ! -z "$MISSING_LIBS" ]; then
    echo -e "${RED}❌ Missing libraries: ${MISSING_LIBS}${NC}"
    echo -e "${YELLOW}💡 On macOS with Homebrew: brew install curl cjson${NC}"
    echo -e "${YELLOW}💡 On Ubuntu/Debian: sudo apt-get install libcurl4-openssl-dev libcjson-dev${NC}"
    exit 1
fi

# Show library versions
echo -e "${GREEN}✅ libcurl:$(pkg-config --modversion libcurl 2>/dev/null || echo ' found')${NC}"
if pkg-config --exists libcjson; then
    echo -e "${GREEN}✅ libcjson:$(pkg-config --modversion libcjson 2>/dev/null || echo ' found')${NC}"
elif pkg-config --exists cjson; then
    echo -e "${GREEN}✅ cjson:$(pkg-config --modversion cjson 2>/dev/null || echo ' found')${NC}"
fi

# Get compilation flags
CFLAGS=$(pkg-config --cflags libcurl)
LIBS=$(pkg-config --libs libcurl)

# Add cjson flags (try both libcjson and cjson)
if pkg-config --exists libcjson; then
    CFLAGS="${CFLAGS} $(pkg-config --cflags libcjson)"
    LIBS="${LIBS} $(pkg-config --libs libcjson)"
elif pkg-config --exists cjson; then
    CFLAGS="${CFLAGS} $(pkg-config --cflags cjson)"
    LIBS="${LIBS} $(pkg-config --libs cjson)"
fi

# Add math library
LIBS="${LIBS} -lm"

echo
echo -e "${BLUE}Building integration tests...${NC}"

# Build integration_test (server-based)
if [ -f "integration_test.c" ]; then
    echo -e "🔧 Building integration_test (server-based)..."
    gcc -o integration_test integration_test.c ${CFLAGS} ${LIBS}
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ integration_test compiled successfully${NC}"
    else
        echo -e "${RED}❌ integration_test compilation failed${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠️  integration_test.c not found, skipping${NC}"
fi

# Build test-integration-local (capabilities demo)
if [ -f "test-integration-local.c" ]; then
    echo -e "🔧 Building test-integration-local (capabilities demo)..."
    gcc -o test-integration-local test-integration-local.c ${CFLAGS} ${LIBS} -Wno-format-extra-args
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ test-integration-local compiled successfully${NC}"
    else
        echo -e "${RED}❌ test-integration-local compilation failed${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠️  test-integration-local.c not found, skipping${NC}"
fi

echo
echo -e "${BLUE}Integration tests built successfully!${NC}"
echo
echo -e "${GREEN}Available executables:${NC}"

if [ -f "integration_test" ]; then
    echo -e "  ${GREEN}•${NC} ./integration_test --url <server-url>"
    echo -e "    ${BLUE}Full server integration test${NC}"
fi

if [ -f "test-integration-local" ]; then
    echo -e "  ${GREEN}•${NC} ./test-integration-local"
    echo -e "    ${BLUE}Local capabilities demonstration${NC}"
fi

if [ -f "c_self_test" ]; then
    echo -e "  ${GREEN}•${NC} ./c_self_test"
    echo -e "    ${BLUE}Complete C SDK validation (4/4 tests)${NC}"
fi

echo
echo -e "${YELLOW}💡 Usage examples:${NC}"
echo -e "  ${BLUE}# Test integration capabilities:${NC}"
echo -e "  ./test-integration-local"
echo
echo -e "  ${BLUE}# Test against local server:${NC}"
echo -e "  ./integration_test --url http://localhost:8000/graphql"
echo
echo -e "  ${BLUE}# Run complete SDK validation:${NC}"
echo -e "  ./c_self_test"
echo
echo -e "${GREEN}🎯 C SDK maintains ultimate native performance leadership!${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════════════${NC}"