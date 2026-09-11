#!/bin/bash
set -e

# Change directory to the root of the project
cd "$(dirname "$0")/.."

echo "Building FIX Order Example..."
cmake -B build -S . > /dev/null
cmake --build build --target fix_order > /dev/null

echo ""
echo "Running FIX Order Example..."
echo "================================="
./build/fix_order
