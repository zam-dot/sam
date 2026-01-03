#!/bin/bash
set -e  # Exit on error

echo "=== Building transpiler ==="
mkdir -p bin output

# Build the transpiler to bin/main
gcc -Wall -Wextra -std=c99 -Ilib \
    main.c \
    lib/arena.c \
    lib/semicolon.c \
    lib/string_transform.c \
    lib/refcount.c \
    lib/safety.c \
    -o bin/main

echo "✓ Transpiler built as bin/main"

echo ""
echo "=== Running transpiler ==="
./bin/main

echo ""
echo "=== Compiling transpiled code ==="
# Compile the output/main.sam.c to bin/main (overwrites transpiler)
gcc -Ilib -o bin/main output/main.sam.c lib/safety.c

echo ""
echo "=== Running program ==="
./bin/main
