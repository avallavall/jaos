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

# D8 demands bit-identical runs across machines. C23 lets the compiler
# contract a*b+c into a fused multiply-add wherever the target offers one,
# and the kernels are made of exactly that pattern — so without this flag
# the same model can produce different bits on aarch64 (baseline FMA) than
# on x86-64 (none). Determinism is a recorded decision; the flag enforces
# the IEEE-exact arithmetic it silently assumed.
FP := -ffp-contract=off

# libm is the only thing JAOS links against beyond libc. Anyone linking
# libjaos.a needs it too.
LDLIBS := -lm

RELEASE_CFLAGS := $(STD) $(WARN) $(FP) -Werror -O2 -g -DNDEBUG
DEV_CFLAGS     := $(STD) $(WARN) $(FP) -Werror -g -Og
ASAN_CFLAGS    := $(DEV_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer

# Vendored test framework: warnings on, -Werror off — not our code, dev-time
# only (D15), never linked into the library. UNITY_INCLUDE_DOUBLE enables the
# double-precision assertions a solver test suite lives on; it must be seen
# both by unity.c and by every test including unity.h.
UNITY_DIR    := tests/vendor/unity
UNITY_DEFS   := -DUNITY_INCLUDE_DOUBLE
UNITY_CFLAGS := $(STD) $(WARN) $(FP) -g -Og $(UNITY_DEFS)

# Tests may include src/jaos_internal.h: white-box assertions on the data
# structures are part of their job.
TEST_INC := $(INC) -Isrc -I$(UNITY_DIR) $(UNITY_DEFS)

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

# Keep intermediate objects; make otherwise deletes and rebuilds them
# between targets.
.SECONDARY:

all: $(LIB)

$(LIB): $(REL_OBJ)
	$(AR) rcs $@ $^

# Two headers for now; switch to -MMD generated deps when src/ grows.
HDRS := include/jaos.h src/jaos_internal.h

$(B)/release/%.o: src/%.c $(HDRS) | $(B)/release
	$(CC) $(RELEASE_CFLAGS) $(INC) -c $< -o $@

$(B)/dev/%.o: src/%.c $(HDRS) | $(B)/dev
	$(CC) $(DEV_CFLAGS) $(INC) -c $< -o $@

$(B)/asan/%.o: src/%.c $(HDRS) | $(B)/asan
	$(CC) $(ASAN_CFLAGS) $(INC) -c $< -o $@

$(B)/dev/unity.o: $(UNITY_DIR)/unity.c | $(B)/dev
	$(CC) $(UNITY_CFLAGS) -I$(UNITY_DIR) -c $< -o $@

$(B)/asan/unity.o: $(UNITY_DIR)/unity.c | $(B)/asan
	$(CC) $(UNITY_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -I$(UNITY_DIR) -c $< -o $@

$(B)/dev/test_%: tests/test_%.c $(DEV_OBJ) $(B)/dev/unity.o $(HDRS) | $(B)/dev
	$(CC) $(DEV_CFLAGS) $(TEST_INC) $< $(DEV_OBJ) $(B)/dev/unity.o -o $@ $(LDLIBS)

$(B)/asan/test_%: tests/test_%.c $(ASAN_OBJ) $(B)/asan/unity.o $(HDRS) | $(B)/asan
	$(CC) $(ASAN_CFLAGS) $(TEST_INC) $< $(ASAN_OBJ) $(B)/asan/unity.o -o $@ $(LDLIBS)

test: $(DEV_TESTS)
	@fail=0; for t in $(DEV_TESTS); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

sanitize: $(ASAN_TESTS)
	@fail=0; for t in $(ASAN_TESTS); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

$(B)/release $(B)/dev $(B)/asan:
	mkdir -p $@

clean:
	rm -rf $(B)
