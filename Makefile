# JAOS build. GNU make + GCC 14, C23, Linux only (DECISIONS.md D1, D14).
# On Windows, run under WSL.
#
# Targets:
#   all       release static library (default)
#   test      build and run the unit suite (dev flags)
#   sanitize  build and run the unit suite under ASan+UBSan
#   bench     build the Netlib acceptance runner (bench/fetch.sh first)
#   compare   time JAOS against the other solvers on one rung of the ladder
#   compare-solvers   fetch, verify and build the competitors, nothing else
#   netlib    fetch the instances if needed, then run the gate
#   netlib-baseline     rewrite what each instance is expected to do
#   netlib-kennington   the Kennington subset (PLAN 2.9 condition 1b)
#   netlib-infeas       the infeasible subset (PLAN 2.9 condition 1c)
#   netlib-kennington-baseline, netlib-infeas-baseline   rewrite those two
#   plato-pds, plato-fome, plato-nug   the fourth set (TODO.md §4), which has
#             no reference optimum and so runs under -e noref
#   plato     all three of them
#   plato-pds-baseline, plato-fome-baseline, plato-nug-baseline   rewrite those
#   pgo       rebuild the library from a profile of it solving real models
#   clean     remove all build output
#
# J=N runs N instances at once in any of the netlib targets. The record comes
# out byte-identical because everything in it is an integer the solver
# computed — but the seconds printed alongside it do not, and the runner says
# so. A time ratio needs J=1.
#
# NATIVE=1 adds -march=native. LTO=0 removes -flto. Both are measured in D62
# and both defaults are what that measurement chose; see README "Build".

# make predefines CC=cc, so ?= would never fire; override only the built-in
# default while still honouring CC given via environment or command line.
ifeq ($(origin CC),default)
CC := gcc-14
endif

# An archive of LTO objects carries its symbols where only the linker plugin
# can see them, and plain `ar` writes an index that includes them only where
# the distribution configured the plugin in. `gcc-ar` loads it itself, so the
# archive is linkable regardless of how the system's binutils were built.
ifeq ($(origin AR),default)
AR := $(subst gcc,gcc-ar,$(notdir $(CC)))
endif

# How many instances the acceptance runner solves at once. One by default:
# the sequential run is the one whose printed seconds mean anything, so the
# faster mode is asked for rather than assumed. Ten is right for a six-core
# machine on the standard set; the Kennington models are large enough that
# memory, not cores, sets the limit — six to eight there.
J ?= 1

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

# What a shipping build is made of. **One set of flags, not two** — a second
# target nobody types is a second target that rots, and the measurement says
# there is nothing to choose between anyway (D62). Every rung below was run
# over the whole standard set with every verdict, iteration count and digest
# unmoved, so none of them is trading an answer for a second:
#
#   -O3 over -O2                     1.0055x   noise
#   + -flto                          1.0330x   the only flag that does anything
#   + -march=native                  1.0072x   over LTO: noise, and unportable
#   PGO on top of -O3 -flto          1.1122x   `make pgo`
#
# -g stays: it costs nothing at run time and it is what the profiler reads,
# which is how four of this milestone's entries were found. -DNDEBUG is what
# removes the assertions. The work counter and the clock cost 0.987x and
# 1.004x — inside the noise in both directions — so neither is worth a
# compile-time switch (D62).
LTO    ?= 1
NATIVE ?= 0

SHIP := -O3
ifeq ($(LTO),1)
SHIP += -flto
endif
ifeq ($(NATIVE),1)
SHIP += -march=native -mtune=native
endif

# Set by the `pgo` target to instrument, then to consume what it recorded.
PGO_CFLAGS ?=

# A hook for sweeping a method constant over a range without editing the
# source between runs — `make EXTRA_CFLAGS=-DPRICE_PARTITIONS_VALUE=4`. Empty
# in every shipping build, and it is a development switch rather than an
# option: which pricing rule runs is the method, and the method is not the
# caller's to choose (D64). Sweeping a constant that must not change a verdict
# is also how three defects were found that 139 instances at one setting did
# not (D39, D47, D72).
EXTRA_CFLAGS ?=

RELEASE_CFLAGS := $(STD) $(WARN) $(FP) -Werror $(SHIP) -g -DNDEBUG $(PGO_CFLAGS) $(EXTRA_CFLAGS)
DEV_CFLAGS     := $(STD) $(WARN) $(FP) -Werror -g -Og $(EXTRA_CFLAGS)
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

.PHONY: all test sanitize configs bench compare-build compare-solvers compare record-check refusals \
	netlib netlib-baseline \
	netlib-kennington \
	netlib-infeas netlib-kennington-baseline netlib-infeas-baseline \
	plato plato-pds plato-fome plato-nug \
	plato-pds-baseline plato-fome-baseline plato-nug-baseline \
	warm warm-kennington primal primal-kennington \
	pgo clean

# Keep intermediate objects; make otherwise deletes and rebuilds them
# between targets.
.SECONDARY:

all: $(LIB)

# The bench runners are compiled by `make test`, and they are not tests.
#
# **They were compiled by nothing the project loop runs, and it cost a full
# cycle.** `bench/primal.c` classified a refusal by matching a string that the
# same commit deleted from `src/`; `make test`, `make sanitize` and `make
# configs` all passed, because not one of them builds the file. The dead match
# survived until a review read it (D191). `bench/warm.c`'s `-O2` break
# (02-99) is the same hole from the other side — it had to be found by hand.
#
# Compiled, not run: running them needs instances fetched from the network,
# which `make test` must never depend on. `-Werror` on a translation unit
# nobody compiles is not a guarantee, and this is the cheapest place to make
# it one.
BENCH_TOOLS := $(B)/bench/run $(B)/bench/warm $(B)/bench/primal

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

# The written record is checked with the code, because a document that
# describes a tree the code no longer is costs more than no document: it is
# found later, by someone who then trusts it. tools/record-check.py says what
# it checks; docs/claims.txt is the list of things the record says do not
# exist, and is the line that fails when one of them lands.
record-check:
	@python3 tools/record-check.py

# Every refusal with a script is re-tested on this tree; the rest are listed
# with the condition to check by hand. Minutes, not seconds: run it when a
# milestone closes and after any change to pricing, the re-entry, presolve's
# families or the LU kernels. bench/refusals.txt is the list.
refusals:
	@mkdir -p $(B)
	@bash tools/refusals.sh

test: record-check $(DEV_TESTS) $(BENCH_TOOLS)
	@fail=0; for t in $(DEV_TESTS); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

sanitize: $(ASAN_TESTS)
	@fail=0; for t in $(ASAN_TESTS); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

# Every build configuration the suite has, each from clean.
#
# `make` decides what to rebuild from file timestamps and does NOT track a
# change in EXTRA_CFLAGS, so `make test EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` right
# after a plain `make test` re-runs the plain binaries and exits 0 while the
# reference build does not compile. That is not a hypothetical: it hid a broken
# fault build once (D153's record) and then hid three broken configurations
# from D151 until 2026-08-19, including the reference build, which
# `jaos-testing` calls the only oracle for output no predicate reads.
#
# `make clean` between the runs is the whole point of this target. It costs
# five full rebuilds, so it is not in `test` — run it before landing anything
# that touches tests/ or a guarded block in src/.
CONFIGS := -DJAOS_NO_PRESOLVE -DJAOS_PRESOLVE_FAULT_OFFBYONE \
           -DJAOS_PRESOLVE_FAULT_WRONGDUAL

configs:
	@fail=0; \
	$(MAKE) --no-print-directory clean >/dev/null; \
	echo "== plain"; $(MAKE) --no-print-directory test >/dev/null || fail=1; \
	for d in $(CONFIGS); do \
	    $(MAKE) --no-print-directory clean >/dev/null; \
	    echo "== $$d"; \
	    $(MAKE) --no-print-directory test EXTRA_CFLAGS=$$d >/dev/null || fail=1; \
	done; \
	$(MAKE) --no-print-directory clean >/dev/null; \
	echo "== sanitize"; $(MAKE) --no-print-directory sanitize >/dev/null || fail=1; \
	if [ $$fail -eq 0 ]; then echo "all 5 configurations build and pass"; \
	else echo "at least one configuration is broken -- re-run it alone to see why"; fi; \
	exit $$fail

# The acceptance runner links the release library exactly as any other
# consumer would — same headers, same archive, no separate build path for
# the thing that judges it. Built on demand, never by `all`: it is a bench
# tool and not part of what JAOS ships.
#
# -Isrc is the one deliberate exception (D-13): jaos_internal.h's own
# jm_presolve_counts and jaos_model's presolve_num_row/col/nz are not, and
# must not become, public API (D64) — but D-13 also requires this runner to
# print what each reduction removed into the record, and the counter that
# says so lives nowhere a public header could reach. This runner is in-tree
# tooling reading the solver it ships beside, the same relationship tests/
# already has to it (TEST_INC below); it is not a caller judged by the same
# rule jaos.h enforces on everyone else.
$(B)/bench/run: bench/run.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) -Isrc $< $(LIB) -o $@ $(LDLIBS)

bench: $(B)/bench/run

# What warm re-solve buys, which the gate cannot say: the gate solves each
# instance once from a fresh load, and that is the case warm starting does
# not touch. Kept out of the gate for that reason and because it reports a
# ratio rather than a verdict (bench/warm.c).
$(B)/bench/warm: bench/warm.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) $< $(LIB) -o $@ $(LDLIBS)

warm: $(B)/bench/warm
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/warm -j $(J) -o bench/results/warm.txt

# What the primal simplex costs against the dual, which the gate cannot say
# either: a cold start is dual feasible and not primal feasible, so the gate
# never enters a primal path at all (TODO.md 0). Kept out of the gate for the
# same reason `warm` is, and it reports a ratio rather than a verdict.
#
# -Isrc for the same deliberate exception bench/run takes, and no wider:
# cfg.force_primal is not public API and must not become it on this schedule.
# bench/primal.c's header comment carries the argument.
$(B)/bench/primal: bench/primal.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) -Isrc $< $(LIB) -o $@ $(LDLIBS)

primal: $(B)/bench/primal
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/primal -j $(J) -o bench/results/primal.txt

primal-kennington: $(B)/bench/primal
	@bench/fetch.sh -m bench/netlib-kennington.manifest \
		-b https://netlib.org/lp/data/kennington -p gz-emps \
		bench/instances-kennington
	@mkdir -p bench/results
	./$(B)/bench/primal -j $(J) -m bench/netlib-kennington.manifest \
		-d bench/instances-kennington \
		-o bench/results/primal-kennington.txt

# The same campaign on the large set. Three solves per instance where the
# gate does two, so it costs about half as much again as `netlib-kennington`.
warm-kennington: $(B)/bench/warm
	@bench/fetch.sh -m bench/netlib-kennington.manifest \
		-b https://netlib.org/lp/data/kennington -p gz-emps \
		bench/instances-kennington
	@mkdir -p bench/results
	./$(B)/bench/warm -j $(J) -m bench/netlib-kennington.manifest \
		-d bench/instances-kennington \
		-o bench/results/warm-kennington.txt

# JAOS as one competitor among several (bench/compare/README.md). Built on
# demand and kept apart from the gate's runner: it reports seconds, which no
# file the gate reads is allowed to contain (D17).
$(B)/bench/jaos_time: bench/compare/jaos_time.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) $< $(LIB) -o $@ $(LDLIBS)

compare-build: $(B)/bench/jaos_time

# Fetch, checksum-verify and build the competitors, then time JAOS against
# them on one rung of the ladder (bench/compare/README.md). Nothing either
# target downloads enters the repository.
compare-solvers:
	@bench/compare/fetch-solvers.sh

compare: $(B)/bench/jaos_time
	@bench/compare/fetch-solvers.sh highs
	@bench/compare/run-compare.sh $(COMPARE_ARGS)

# Instances are fetched and checksum-verified, never committed (PLAN 2.10).
# The runner writes the record itself rather than being piped through tee:
# a pipeline would report tee's exit status, and a gate that cannot fail the
# build is not a gate.
netlib: $(B)/bench/run
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -o bench/results/netlib.txt -b bench/netlib.baseline

# Rewrites what every instance is expected to do. Separate from `netlib`, and
# never a side effect of it: a baseline that updates itself records whatever
# just happened as correct, which is the one thing it must not do.
netlib-baseline: $(B)/bench/run
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -o bench/results/netlib.txt -w bench/netlib.baseline

# The other two sets the M1 gate asks for (PLAN 2.9). Both are served by
# netlib in its packed form and expanded with emps, which fetch.sh downloads
# and checksum-verifies rather than storing (PLAN Q6, decided 2026-08-07).
#
# Separate instance directories, not one shared: `greenbea` names a feasible
# model in the standard set and a different, infeasible one in this set, and
# two models must never share a path.
#
# Both are diffed per instance against their own baseline, for the same
# reason the standard set is (D21): these two gates report PASS, and a gate
# that already passes is exactly the one whose summary line cannot show a
# change. An instance that still ends INFEASIBLE after eighty times the work
# has regressed, and only the baseline says so.
netlib-kennington: $(B)/bench/run
	@bench/fetch.sh -m bench/netlib-kennington.manifest \
		-b https://netlib.org/lp/data/kennington -p gz-emps \
		bench/instances-kennington
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/netlib-kennington.manifest \
		-d bench/instances-kennington \
		-b bench/netlib-kennington.baseline \
		-o bench/results/netlib-kennington.txt

netlib-infeas: $(B)/bench/run
	@bench/fetch.sh -m bench/netlib-infeas.manifest \
		-b https://netlib.org/lp/infeas -p emps \
		bench/instances-infeas
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/netlib-infeas.manifest -e infeasible \
		-d bench/instances-infeas \
		-b bench/netlib-infeas.baseline \
		-o bench/results/netlib-infeas.txt

# Rewriting those two, kept apart from running them for the reason
# netlib-baseline is kept apart from netlib.
netlib-kennington-baseline: $(B)/bench/run
	@bench/fetch.sh -m bench/netlib-kennington.manifest \
		-b https://netlib.org/lp/data/kennington -p gz-emps \
		bench/instances-kennington
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/netlib-kennington.manifest \
		-d bench/instances-kennington \
		-w bench/netlib-kennington.baseline \
		-o bench/results/netlib-kennington.txt

netlib-infeas-baseline: $(B)/bench/run
	@bench/fetch.sh -m bench/netlib-infeas.manifest \
		-b https://netlib.org/lp/infeas -p emps \
		bench/instances-infeas
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/netlib-infeas.manifest -e infeasible \
		-d bench/instances-infeas \
		-w bench/netlib-infeas.baseline \
		-o bench/results/netlib-infeas.txt

# The fourth set (TODO.md §4). Three families from Mittelmann's mirror, all
# in netlib's emps packing but bzip2'd, and all carrying `none` in the
# manifest's source column: nobody has published an exact optimum for them, so
# they run under `-e noref` and the runner refuses the pairing if either half
# disagrees (bench/measurements/02-22/).
#
# **Not part of the gate.** The gate is the three netlib* targets and stays
# that way until this set has a baseline anyone has read. What it is for is the
# question every verdict in this repository already depends on and none of them
# tested: whether a conclusion taken on 139 small models survives a population
# that is not netlib.
plato: plato-pds plato-fome plato-nug

plato-pds: $(B)/bench/run
	@bench/fetch.sh -m bench/plato-pds.manifest \
		-b https://plato.asu.edu/ftp/lptestset/pds -p bz2-emps \
		bench/instances-plato-pds
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/plato-pds.manifest -e noref \
		-d bench/instances-plato-pds \
		-b bench/plato-pds.baseline \
		-o bench/results/plato-pds.txt

plato-fome: $(B)/bench/run
	@bench/fetch.sh -m bench/plato-fome.manifest \
		-b https://plato.asu.edu/ftp/lptestset/fome -p bz2-emps \
		bench/instances-plato-fome
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/plato-fome.manifest -e noref \
		-d bench/instances-plato-fome \
		-b bench/plato-fome.baseline \
		-o bench/results/plato-fome.txt

plato-nug: $(B)/bench/run
	@bench/fetch.sh -m bench/plato-nug.manifest \
		-b https://plato.asu.edu/ftp/lptestset/nug -p bz2-emps \
		bench/instances-plato-nug
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/plato-nug.manifest -e noref \
		-d bench/instances-plato-nug \
		-b bench/plato-nug.baseline \
		-o bench/results/plato-nug.txt

# Rewriting those three, kept apart from running them for the reason
# netlib-baseline is kept apart from netlib.
plato-pds-baseline: $(B)/bench/run
	@bench/fetch.sh -m bench/plato-pds.manifest \
		-b https://plato.asu.edu/ftp/lptestset/pds -p bz2-emps \
		bench/instances-plato-pds
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/plato-pds.manifest -e noref \
		-d bench/instances-plato-pds \
		-w bench/plato-pds.baseline \
		-o bench/results/plato-pds.txt

plato-fome-baseline: $(B)/bench/run
	@bench/fetch.sh -m bench/plato-fome.manifest \
		-b https://plato.asu.edu/ftp/lptestset/fome -p bz2-emps \
		bench/instances-plato-fome
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/plato-fome.manifest -e noref \
		-d bench/instances-plato-fome \
		-w bench/plato-fome.baseline \
		-o bench/results/plato-fome.txt

plato-nug-baseline: $(B)/bench/run
	@bench/fetch.sh -m bench/plato-nug.manifest \
		-b https://plato.asu.edu/ftp/lptestset/nug -p bz2-emps \
		bench/instances-plato-nug
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/plato-nug.manifest -e noref \
		-d bench/instances-plato-nug \
		-w bench/plato-nug.baseline \
		-o bench/results/plato-nug.txt

# Profile-guided rebuild: compile instrumented, solve real models with it,
# then compile again with what that recorded. Worth 1.1122x over the plain
# shipping build on the timed set, which is three times what every flag in it
# is worth put together (D62).
#
# **Not what `make` does**, deliberately. It takes minutes rather than
# seconds, and it cannot run at all until the instances have been fetched,
# which needs the network. A library that will not build without downloading
# 139 models from netlib is a library nobody can package.
#
# The load is the standard set, sequential. Sequential because each worker
# process of `-j` would be writing the same .gcda files at the same time;
# the whole set because the profile should describe the models JAOS is for,
# and 94 real ones are already there to be described. PGO_LOAD overrides it
# with a subset when a faster turnaround is wanted.
PGO_DIR  := $(abspath $(B)/pgo)
PGO_LOAD ?=

.PHONY: pgo
pgo:
	@bench/fetch.sh
	@echo "== PGO 1/3: instrumented build"
	@rm -rf $(B)/release $(B)/bench $(PGO_DIR)
	@mkdir -p $(PGO_DIR)
	$(MAKE) --no-print-directory \
		PGO_CFLAGS="-fprofile-generate -fprofile-dir=$(PGO_DIR)" $(B)/bench/run
	@echo "== PGO 2/3: solving $(if $(PGO_LOAD),$(words $(PGO_LOAD)) instances,the standard set) to record a profile"
	@./$(B)/bench/run -o /dev/null $(PGO_LOAD) > $(B)/pgo-load.log 2>&1 || true
	@echo "   $$(ls $(PGO_DIR)/*.gcda 2>/dev/null | wc -l) profile files"
	@echo "== PGO 3/3: rebuilding from the profile"
	@rm -rf $(B)/release $(B)/bench
	$(MAKE) --no-print-directory \
		PGO_CFLAGS="-fprofile-use -fprofile-correction -fprofile-dir=$(PGO_DIR) -Wno-missing-profile" \
		all
	@echo "== $(LIB) is now built from a profile of $(if $(PGO_LOAD),$(words $(PGO_LOAD)),94) real models"

$(B)/release $(B)/dev $(B)/asan $(B)/bench:
	mkdir -p $@

clean:
	rm -rf $(B)
