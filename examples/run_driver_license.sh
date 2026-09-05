#!/bin/bash
set -e

# Change to the directory of the script
cd "$(dirname "$0")"

echo "Building Driver License Example..."
# Ensure g++-16 is available. P2996 features require g++ with -freflection
g++-16 -std=c++26 -freflection -I../include driver_license.cpp -o driver_license

echo ""
echo "Running Driver License Example..."
echo "================================="
./driver_license
