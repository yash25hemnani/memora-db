# Compiler flags: enable warnings, optimize, use the C23 standard
flags=-Wall -O2 -std=c2x
# Extra linker flags (none by default)
ldflags=

# Default target: build the executable
all: clean tree

# Link the object file into the final executable
tree: tree.o
	cc ${flags} $^ -o $@ ${ldflags}

# Compile the source file into an object file
tree.o: tree.c
	cc ${flags} -c $^

# Remove build artifacts
clean:
	rm -f *.o tree
