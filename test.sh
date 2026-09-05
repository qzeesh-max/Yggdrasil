#!/bin/bash
set -e

# Run the build script first to ensure tests are up to date
./build.sh

echo "Running tests..."
cd build
ctest --output-on-failure
cd ..
