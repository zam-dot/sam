CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Ilib
PROGRAM = bin/main
OUTPUT = output/main.sam.c

all: run

# Direct compilation: transpile and compile in one step
$(PROGRAM): main.c lib/*.c src/main.sam
	mkdir -p bin output
	
	# Step 1: Compile the transpiler
	$(CC) $(CFLAGS) main.c lib/semicolon.c lib/string_transform.c \
	    lib/refcount.c lib/safety.c -o bin/transpiler-temp
	
	# Step 2: Run transpiler to create output
	./bin/transpiler-temp
	
	# Step 3: Compile the transpiled output
	$(CC) -Ilib -o $(PROGRAM) $(OUTPUT) lib/safety.c
	
	# Step 4: Clean up temp transpiler
	rm -f bin/transpiler-temp

run: $(PROGRAM)
	./$(PROGRAM)

clean:
	rm -rf bin output

.PHONY: all run clean
