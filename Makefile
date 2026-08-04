# JAOS build. GNU make + GCC 14, C23, Linux only (DECISIONS.md D1, D14).
# On Windows, run under WSL.
#
# Targets:
#   all       release static library (default)
#   test      build and run the unit suite (dev flags)
#   sanitize  build and run the unit suite under ASan+UBSan
#   clean     remove all build output

# make predefines CC=cc, so ?= would never fire; override only the built-in
# default while still honouring CC given via environment or command line.
ifeq ($(origin CC),default)
CC := gcc-14
endif
AR ?= ar

STD  := -std=c23
WARN := -Wall -Wextra -Wpedantic
INC  := -Iinclude

RELEASE_CFLAGS := $(STD) $(WARN) -Werror -O2 -g -DNDEBUG
DEV_CFLAGS     := $(STD) $(WARN) -Werror -g -Og
ASAN_CFLAGS    := $(DEV_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer

# Vendored test framework: warnings on, -Werror off — not our code, dev-time
# only (D15), never linked into the library.
UNITY_DIR    := tests/vendor/unity
UNITY_CFLAGS := $(STD) $(WARN) -g -Og

SRC   := $(wildcard src/*.c)
TESTS := $(wildcard tests/test_*.c)

B := build

REL_OBJ  := $(SRC:src/%.c=$(B)/release/%.o)
DEV_OBJ  := $(SRC:src/%.c=$(B)/dev/%.o)
ASAN_OBJ := $(SRC:src/%.c=$(B)/asan/%.o)

LIB := $(B)/release/libjaos.a

DEV_TESTS  := $(TESTS:tests/%.c=$(B)/dev/%)
ASAN_TESTS := $(TESTS:tests/%.c=$(B)/asan/%)

.PHONY: all test sanitize clean

all: $(LIB)

$(LIB): $(REL_OBJ)
	$(AR) rcs $@ $^

# Single public header for now; switch to -MMD generated deps when src/ grows.
$(B)/release/%.o: src/%.c include/jaos.h | $(B)/release
	$(CC) $(RELEASE_CFLAGS) $(INC) -c $< -o $@

$(B)/dev/%.o: src/%.c include/jaos.h | $(B)/dev
	$(CC) $(DEV_CFLAGS) $(INC) -c $< -o $@

$(B)/asan/%.o: src/%.c include/jaos.h | $(B)/asan
	$(CC) $(ASAN_CFLAGS) $(INC) -c $< -o $@

$(B)/dev/unity.o: $(UNITY_DIR)/unity.c | $(B)/dev
	$(CC) $(UNITY_CFLAGS) -I$(UNITY_DIR) -c $< -o $@

$(B)/asan/unity.o: $(UNITY_DIR)/unity.c | $(B)/asan
	$(CC) $(UNITY_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -I$(UNITY_DIR) -c $< -o $@

$(B)/dev/test_%: tests/test_%.c $(DEV_OBJ) $(B)/dev/unity.o | $(B)/dev
	$(CC) $(DEV_CFLAGS) $(INC) -I$(UNITY_DIR) $< $(DEV_OBJ) $(B)/dev/unity.o -o $@

$(B)/asan/test_%: tests/test_%.c $(ASAN_OBJ) $(B)/asan/unity.o | $(B)/asan
	$(CC) $(ASAN_CFLAGS) $(INC) -I$(UNITY_DIR) $< $(ASAN_OBJ) $(B)/asan/unity.o -o $@

test: $(DEV_TESTS)
	@fail=0; for t in $(DEV_TESTS); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

sanitize: $(ASAN_TESTS)
	@fail=0; for t in $(ASAN_TESTS); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

$(B)/release $(B)/dev $(B)/asan:
	mkdir -p $@

clean:
	rm -rf $(B)
