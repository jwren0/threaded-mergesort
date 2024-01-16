## User configuration ##
CC ?= cc
CCFLAGS ?= -march=native -O2 -pipe

EXEC := mergesort


## Developer configuration ##
CCFLAGS := $(CCFLAGS) -Wall -Wextra -Werror -Wformat-security \
		-Wpedantic -pedantic-errors -std=c18

SRC_FILES = $(shell find src/ -name "*.c")
OBJ_FILES = ${SRC_FILES:.c=.o}


## User targets ##
.PHONY: build build_debug clean

build: $(EXEC)
build_debug: $(EXEC)_debug

clean:
	@rm -f $(OBJ_FILES) $(EXEC) $(EXEC)_debug


## Developer targets ##
$(EXEC): $(OBJ_FILES)
	@printf "CC      $@\n"
	@$(CC) $(CCFLAGS) -lpthread -Xlinker --strip-all -o $@ $^

$(EXEC)_debug: $(OBJ_FILES)
	@printf "CC      $@\n"
	@$(CC) $(CCFLAGS) -lpthread -g -o $@ $^

src/%.o: src/%.c
	@printf "CC      $@\n"
	@$(CC) $(CCFLAGS) -c -g -o $@ $<
