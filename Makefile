CC      = cc
CFLAGS  = -Wall -O2 -std=c2x -Iinclude -MMD -MP
LDFLAGS = -lcrypt

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
DEP = $(OBJ:.o=.d)

.PHONY: all clean

all: build/memora

build/memora: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

-include $(DEP)