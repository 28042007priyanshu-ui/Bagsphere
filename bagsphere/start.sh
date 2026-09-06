#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
mkdir -p build

echo "[1/2] Compiling C FIFO queue..."
gcc -c reports_queue.c -o build/reports_queue.o

echo "[2/2] Compiling C++ BagSphere server..."
g++ -std=c++17 server.cpp build/reports_queue.o -o build/bagsphere

echo
echo "BagSphere is starting at http://localhost:8000"
echo "Press Ctrl+C to stop."
./build/bagsphere
