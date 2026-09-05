#!/bin/bash
set -e

echo "Building Yggdrasil..."
# Ensure cmake runs with g++-16 which supports -freflection (P2996)
CXX=g++-16 cmake -B build -S .
CXX=g++-16 cmake --build build

echo "Build complete."
