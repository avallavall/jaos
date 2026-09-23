ifeq ($(origin CC),default)
CC := gcc-14
endif

ifeq ($(origin AR),default)
AR := $(subst gcc,gcc-ar,$(notdir $(CC)))
endif

J ?= 1

STD  := -std=c23
WARN := -Wall -Wextra -Wpedantic
INC  := -Iinclude

LIBONLY := -fvisibility=hidden -DJAOS_BUILD

HASH := \#
JAOS_VERSION := $(shell sed -n 's/^$(HASH)define JAOS_VERSION_STRING "\(.*\)"/\1/p' include/jaos.h)
JAOS_MAJOR   := $(shell sed -n 's/^$(HASH)define JAOS_VERSION_MAJOR \([0-9]*\)/\1/p' include/jaos.h)

FP := -ffp-contract=off

THREADS := -pthread

LDLIBS := -lm -pthread

LTO    ?= 1
NATIVE ?= 0

SHIP := -O3
ifeq ($(LTO),1)
SHIP += -flto
endif
ifeq ($(NATIVE),1)
SHIP += -march=native -mtune=native
endif

PGO_CFLAGS ?=

EXTRA_CFLAGS ?=

RELEASE_CFLAGS := $(STD) $(WARN) $(FP) $(THREADS) -Werror $(SHIP) -g -DNDEBUG $(PGO_CFLAGS) $(EXTRA_CFLAGS)
DEV_CFLAGS     := $(STD) $(WARN) $(FP) $(THREADS) -Werror -g -Og $(EXTRA_CFLAGS)
ASAN_CFLAGS    := $(DEV_CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
COV_CFLAGS     := $(STD) $(WARN) $(FP) $(THREADS) -Werror -g -O0 --coverage $(EXTRA_CFLAGS)

GCOV     ?= gcov-14
VALGRIND := valgrind -q --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite

UNITY_DIR    := tests/vendor/unity
UNITY_DEFS   := -DUNITY_INCLUDE_DOUBLE
UNITY_CFLAGS := $(STD) $(WARN) $(FP) -g -Og $(UNITY_DEFS)

TEST_INC := $(INC) -Isrc -I$(UNITY_DIR) $(UNITY_DEFS)

SRC   := $(wildcard src/*.c)
TESTS := $(wildcard tests/test_*.c)

B := build

REL_OBJ  := $(SRC:src/%.c=$(B)/release/%.o)
DEV_OBJ  := $(SRC:src/%.c=$(B)/dev/%.o)
ASAN_OBJ := $(SRC:src/%.c=$(B)/asan/%.o)
PIC_OBJ  := $(SRC:src/%.c=$(B)/pic/%.o)
COV_OBJ  := $(SRC:src/%.c=$(B)/cov/%.o)

LIB := $(B)/release/libjaos.a

SHLIB := $(B)/release/libjaos.so

DEV_TESTS  := $(TESTS:tests/%.c=$(B)/dev/%)
ASAN_TESTS := $(TESTS:tests/%.c=$(B)/asan/%)
COV_TESTS  := $(TESTS:tests/%.c=$(B)/cov/%)

.PHONY: all test docs-check version-check sdist-test sanitize coverage valgrind configs cli bench compare-build compare-solvers compare refusals \
	install uninstall pkgconfig exports-test install-test cmake-test windows-test \
	netlib netlib-baseline \
	netlib-kennington \
	netlib-infeas netlib-kennington-baseline netlib-infeas-baseline \
	plato plato-pds plato-fome plato-nug \
	plato-pds-baseline plato-fome-baseline plato-nug-baseline \
	miplib miplib-baseline miplib2017 cblib cblib-baseline \
	warm warm-kennington primal primal-kennington barrier barrier-infeas \
	pdlp pdlp-infeas concurrent \
	shared python-test julia-test dotnet-test java java-test r-test fuzz \
	pgo clean

.SECONDARY:

all: $(LIB)

BENCH_TOOLS := $(B)/bench/run $(B)/bench/warm $(B)/bench/primal $(B)/bench/barrier

CLI := $(B)/cli/jaos

$(LIB): $(REL_OBJ)
	$(AR) rcs $@ $^

HDRS := include/jaos.h src/jaos_internal.h

$(B)/release/%.o: src/%.c $(HDRS) | $(B)/release
	$(CC) $(RELEASE_CFLAGS) $(LIBONLY) $(INC) -c $< -o $@

$(B)/dev/%.o: src/%.c $(HDRS) | $(B)/dev
	$(CC) $(DEV_CFLAGS) $(LIBONLY) $(INC) -c $< -o $@

$(B)/asan/%.o: src/%.c $(HDRS) | $(B)/asan
	$(CC) $(ASAN_CFLAGS) $(LIBONLY) $(INC) -c $< -o $@

$(B)/pic/%.o: src/%.c $(HDRS) | $(B)/pic
	$(CC) $(RELEASE_CFLAGS) -fPIC $(LIBONLY) $(INC) -c $< -o $@

$(SHLIB): $(PIC_OBJ) | $(B)/release
	$(CC) $(RELEASE_CFLAGS) -shared -Wl,-soname,libjaos.so.$(JAOS_MAJOR) -o $@ $^ $(LDLIBS)

SHLINK := $(B)/release/libjaos.so.$(JAOS_MAJOR)

$(SHLINK): $(SHLIB)
	ln -sf libjaos.so $@

shared: $(SHLIB) $(SHLINK)

$(B)/dev/unity.o: $(UNITY_DIR)/unity.c | $(B)/dev
	$(CC) $(UNITY_CFLAGS) -I$(UNITY_DIR) -c $< -o $@

$(B)/asan/unity.o: $(UNITY_DIR)/unity.c | $(B)/asan
	$(CC) $(UNITY_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -I$(UNITY_DIR) -c $< -o $@

$(B)/dev/test_%: tests/test_%.c $(DEV_OBJ) $(B)/dev/unity.o $(HDRS) | $(B)/dev
	$(CC) $(DEV_CFLAGS) $(TEST_INC) $< $(DEV_OBJ) $(B)/dev/unity.o -o $@ $(LDLIBS)

$(B)/asan/test_%: tests/test_%.c $(ASAN_OBJ) $(B)/asan/unity.o $(HDRS) | $(B)/asan
	$(CC) $(ASAN_CFLAGS) $(TEST_INC) $< $(ASAN_OBJ) $(B)/asan/unity.o -o $@ $(LDLIBS)

$(B)/cov/%.o: src/%.c $(HDRS) | $(B)/cov
	$(CC) $(COV_CFLAGS) $(LIBONLY) $(INC) -c $< -o $@

$(B)/cov/unity.o: $(UNITY_DIR)/unity.c | $(B)/cov
	$(CC) $(UNITY_CFLAGS) -I$(UNITY_DIR) -c $< -o $@

$(B)/cov/test_%: tests/test_%.c $(COV_OBJ) $(B)/cov/unity.o $(HDRS) | $(B)/cov
	$(CC) $(COV_CFLAGS) $(TEST_INC) $< $(COV_OBJ) $(B)/cov/unity.o -o $@ $(LDLIBS)

refusals:
	@mkdir -p $(B)
	@bash tools/refusals.sh

test: $(DEV_TESTS) $(BENCH_TOOLS) $(CLI) exports-test install-test cmake-test windows-test
	@fail=0; for t in $(DEV_TESTS); do echo "== $$t"; ./$$t || fail=1; done; \
	echo "== tests/cli.sh"; JAOS_CLI_TEST_FLAGS='$(EXTRA_CFLAGS)' bash tests/cli.sh $(CLI) || fail=1; \
	echo "== tools/docs-check.sh"; bash tools/docs-check.sh || fail=1; \
	echo "== tools/version-check.sh"; bash tools/version-check.sh || fail=1; exit $$fail

docs-check:
	@bash tools/docs-check.sh

version-check:
	@bash tools/version-check.sh

sdist-test:
	@echo "== tests/sdist.sh"; bash tests/sdist.sh

exports-test: $(SHLIB)
	@echo "== tests/exports.sh"; bash tests/exports.sh $(SHLIB)

install-test: $(LIB) $(SHLIB) $(CLI) $(B)/jaos.pc
	@echo "== tests/install.sh"; bash tests/install.sh $(CC)

cmake-test:
	@echo "== tests/cmake.sh"; bash tests/cmake.sh $(CC)

windows-test: $(CLI)
	@echo "== tests/windows.sh"; \
	JAOS_WINDOWS_TEST_FLAGS='$(EXTRA_CFLAGS)' bash tests/windows.sh

sanitize: $(ASAN_TESTS)
	@fail=0; for t in $(ASAN_TESTS); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

coverage: $(COV_TESTS)
	@rm -f $(B)/cov/*.gcda
	@fail=0; for t in $(COV_TESTS); do ./$$t > $$t.log 2>&1 || { echo "FAIL $$t"; fail=1; }; done; \
	bash tools/coverage.sh $(GCOV) $(B)/cov; exit $$fail

valgrind: $(DEV_TESTS)
	@fail=0; for t in $(DEV_TESTS); do echo "== $$t"; $(VALGRIND) ./$$t || fail=1; done; exit $$fail

python-test: $(SHLIB)
	@JAOS_LIBRARY=$(CURDIR)/$(SHLIB) python3 -m unittest discover -s python -v

julia-test: $(SHLIB)
	@julia --project=julia/JAOS -e 'using Pkg; Pkg.instantiate()'
	@JAOS_LIBRARY=$(CURDIR)/$(SHLIB) julia --project=julia/JAOS julia/JAOS/test/runtests.jl

dotnet-test: $(SHLIB)
	@DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 \
		JAOS_LIBRARY=$(CURDIR)/$(SHLIB) dotnet run --project dotnet/Jaos.Check

JAVAC := javac -Xlint:all,-restricted -Werror

java:
	@rm -rf $(B)/java
	@$(JAVAC) -d $(B)/java/classes java/src/org/jaos/*.java
	@jar --create --file $(B)/java/jaos.jar -C $(B)/java/classes .

r-test: $(SHLIB) $(SHLINK)
	@mkdir -p $(B)/R
	@JAOS_INCLUDE=$(CURDIR)/include JAOS_LIB_DIR=$(CURDIR)/$(B)/release \
		R CMD INSTALL --clean --library=$(B)/R R/jaos > $(B)/R/install.log 2>&1 \
		|| (cat $(B)/R/install.log; false)
	@R_LIBS=$(CURDIR)/$(B)/R Rscript R/check.R tests/data

java-test: java $(SHLIB)
	@$(JAVAC) -cp $(B)/java/classes -d $(B)/java/check java/check/Check.java
	@JAOS_LIBRARY=$(CURDIR)/$(SHLIB) java --enable-native-access=ALL-UNNAMED \
		-cp $(B)/java/jaos.jar:$(B)/java/check Check tests/data

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

$(B)/bench/run: bench/run.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) -Isrc $< $(LIB) -o $@ $(LDLIBS)

bench: $(B)/bench/run

$(CLI): cli/jaos.c $(LIB) | $(B)/cli
	$(CC) $(RELEASE_CFLAGS) $(INC) $< $(LIB) -o $@ $(LDLIBS)

cli: $(CLI)

$(B)/bench/warm: bench/warm.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) $< $(LIB) -o $@ $(LDLIBS)

warm: $(B)/bench/warm
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/warm -j $(J) -o bench/results/warm.txt

$(B)/bench/primal: bench/primal.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) -Isrc $< $(LIB) -o $@ $(LDLIBS)

primal: $(B)/bench/primal
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/primal -j $(J) -o bench/results/primal.txt

$(B)/bench/barrier: bench/barrier.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) -Isrc $< $(LIB) -o $@ $(LDLIBS)

barrier: $(B)/bench/barrier
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/barrier -j $(J) -o bench/results/barrier.txt

pdlp: $(B)/bench/barrier
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/barrier -a pdlp -j $(J) -o bench/results/pdlp.txt

concurrent: $(B)/bench/barrier
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/barrier -a concurrent -j $(J) \
		-o bench/results/concurrent.txt

pdlp-infeas: $(B)/bench/barrier
	@bench/fetch.sh -m bench/netlib-infeas.manifest \
		-b https://netlib.org/lp/infeas -p emps \
		bench/instances-infeas
	@mkdir -p bench/results
	./$(B)/bench/barrier -a pdlp -j $(J) -m bench/netlib-infeas.manifest \
		-d bench/instances-infeas \
		-o bench/results/pdlp-infeas.txt

barrier-infeas: $(B)/bench/barrier
	@bench/fetch.sh -m bench/netlib-infeas.manifest \
		-b https://netlib.org/lp/infeas -p emps \
		bench/instances-infeas
	@mkdir -p bench/results
	./$(B)/bench/barrier -j $(J) -m bench/netlib-infeas.manifest \
		-d bench/instances-infeas \
		-o bench/results/barrier-infeas.txt

primal-kennington: $(B)/bench/primal
	@bench/fetch.sh -m bench/netlib-kennington.manifest \
		-b https://netlib.org/lp/data/kennington -p gz-emps \
		bench/instances-kennington
	@mkdir -p bench/results
	./$(B)/bench/primal -j $(J) -m bench/netlib-kennington.manifest \
		-d bench/instances-kennington \
		-o bench/results/primal-kennington.txt

warm-kennington: $(B)/bench/warm
	@bench/fetch.sh -m bench/netlib-kennington.manifest \
		-b https://netlib.org/lp/data/kennington -p gz-emps \
		bench/instances-kennington
	@mkdir -p bench/results
	./$(B)/bench/warm -j $(J) -m bench/netlib-kennington.manifest \
		-d bench/instances-kennington \
		-o bench/results/warm-kennington.txt

$(B)/bench/jaos_time: bench/compare/jaos_time.c $(LIB) | $(B)/bench
	$(CC) $(RELEASE_CFLAGS) $(INC) $< $(LIB) -o $@ $(LDLIBS)

compare-build: $(B)/bench/jaos_time

compare-solvers:
	@bench/compare/fetch-solvers.sh

compare: $(B)/bench/jaos_time
	@bench/compare/fetch-solvers.sh highs
	@bench/compare/run-compare.sh $(COMPARE_ARGS)

netlib: $(B)/bench/run
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -o bench/results/netlib.txt -b bench/netlib.baseline

netlib-baseline: $(B)/bench/run
	@bench/fetch.sh
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -o bench/results/netlib.txt -w bench/netlib.baseline

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

plato: plato-pds plato-fome

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
		-o bench/results/plato-nug.txt

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

miplib: $(B)/bench/run
	@bench/fetch.sh -m bench/miplib.manifest \
		-b https://miplib2010.zib.de/miplib3/miplib3 -p mps-gz \
		bench/instances-miplib
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/miplib.manifest -e mip \
		-d bench/instances-miplib \
		-b bench/miplib.baseline \
		-o bench/results/miplib.txt

MIPLIB2017_WORK ?= 1e10
MIPLIB2017_ARGS ?=
MIPLIB2017_OUT  ?= bench/results/miplib2017.txt

miplib2017: $(B)/bench/run
	@bench/fetch.sh -m bench/miplib2017.manifest \
		-b https://miplib.zib.de/WebData/instances -p mps-gz \
		bench/instances-miplib2017
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/miplib2017.manifest -e mip \
		-d bench/instances-miplib2017 -L $(MIPLIB2017_WORK) $(MIPLIB2017_ARGS) \
		-o $(MIPLIB2017_OUT)

maros-meszaros: $(B)/bench/run
	@bench/fetch.sh -m bench/maros-meszaros.manifest \
		-b https://raw.githubusercontent.com/YimingYAN/QP-Test-Problems/master/QPS_Files \
		-p qps bench/instances-maros-meszaros
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/maros-meszaros.manifest \
		-d bench/instances-maros-meszaros \
		-b bench/maros-meszaros.baseline \
		-o bench/results/maros-meszaros.txt

maros-meszaros-baseline: $(B)/bench/run
	@bench/fetch.sh -m bench/maros-meszaros.manifest \
		-b https://raw.githubusercontent.com/YimingYAN/QP-Test-Problems/master/QPS_Files \
		-p qps bench/instances-maros-meszaros
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/maros-meszaros.manifest \
		-d bench/instances-maros-meszaros \
		-w bench/maros-meszaros.baseline \
		-o bench/results/maros-meszaros.txt

cblib: $(B)/bench/run
	@bench/fetch.sh -m bench/cblib.manifest \
		-b https://cblib.zib.de/download/cblib2014/cont \
		-p cbf-gz bench/instances-cblib
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/cblib.manifest -x cbf.gz \
		-d bench/instances-cblib \
		-b bench/cblib.baseline \
		-o bench/results/cblib.txt

cblib-baseline: $(B)/bench/run
	@bench/fetch.sh -m bench/cblib.manifest \
		-b https://cblib.zib.de/download/cblib2014/cont \
		-p cbf-gz bench/instances-cblib
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/cblib.manifest -x cbf.gz \
		-d bench/instances-cblib \
		-w bench/cblib.baseline \
		-o bench/results/cblib.txt

miplib-baseline: $(B)/bench/run
	@bench/fetch.sh -m bench/miplib.manifest \
		-b https://miplib2010.zib.de/miplib3/miplib3 -p mps-gz \
		bench/instances-miplib
	@mkdir -p bench/results
	./$(B)/bench/run -j $(J) -m bench/miplib.manifest -e mip \
		-d bench/instances-miplib \
		-w bench/miplib.baseline \
		-o bench/results/miplib.txt

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

$(B)/release $(B)/dev $(B)/asan $(B)/cov $(B)/bench $(B)/cli $(B)/pic $(B)/fuzz:
	mkdir -p $@

FUZZ_CC      ?= clang-20
FUZZ_CFLAGS  := -std=c23 -g -O1 -ffp-contract=off -pthread \
                -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined
FUZZ_FORMATS := mps lp nl osil qplib cbf

$(B)/fuzz/fuzz_%: tests/fuzz_readers.c $(SRC) $(HDRS) | $(B)/fuzz
	$(FUZZ_CC) $(FUZZ_CFLAGS) -DJAOS_FUZZ_FORMAT='"$*"' $(INC) -Isrc \
		tests/fuzz_readers.c $(SRC) -o $@ -lm

fuzz: $(FUZZ_FORMATS:%=$(B)/fuzz/fuzz_%)

clean:
	rm -rf $(B)

PREFIX     ?= /usr/local
DESTDIR    ?=
BINDIR     ?= $(PREFIX)/bin
LIBDIR     ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
INSTALL    ?= install


$(B)/jaos.pc: include/jaos.h Makefile | $(B)/release
	@printf 'prefix=%s\n' '$(PREFIX)'                          >  $@
	@printf 'exec_prefix=$${prefix}\n'                         >> $@
	@printf 'libdir=%s\n' '$(LIBDIR)'                          >> $@
	@printf 'includedir=%s\n' '$(INCLUDEDIR)'                  >> $@
	@printf '\n'                                               >> $@
	@printf 'Name: jaos\n'                                     >> $@
	@printf 'Description: Just Another Optimization Solver\n'  >> $@
	@printf 'URL: https://github.com/avallavall/jaos\n'        >> $@
	@printf 'Version: %s\n' '$(JAOS_VERSION)'                  >> $@
	@printf 'Libs: -L$${libdir} -ljaos\n'                      >> $@
	@printf 'Libs.private: -lm -pthread\n'                     >> $@
	@printf 'Cflags: -I$${includedir}\n'                       >> $@

pkgconfig: $(B)/jaos.pc

install: $(LIB) $(SHLIB) $(CLI) $(B)/jaos.pc
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(LIBDIR) \
		$(DESTDIR)$(BINDIR) $(DESTDIR)$(PKGCONFIGDIR)
	$(INSTALL) -m 644 include/jaos.h $(DESTDIR)$(INCLUDEDIR)/jaos.h
	$(INSTALL) -m 644 $(LIB) $(DESTDIR)$(LIBDIR)/libjaos.a
	$(INSTALL) -m 755 $(SHLIB) $(DESTDIR)$(LIBDIR)/libjaos.so.$(JAOS_VERSION)
	ln -sf libjaos.so.$(JAOS_VERSION) $(DESTDIR)$(LIBDIR)/libjaos.so.$(JAOS_MAJOR)
	ln -sf libjaos.so.$(JAOS_MAJOR) $(DESTDIR)$(LIBDIR)/libjaos.so
	$(INSTALL) -m 755 $(CLI) $(DESTDIR)$(BINDIR)/jaos
	$(INSTALL) -m 644 $(B)/jaos.pc $(DESTDIR)$(PKGCONFIGDIR)/jaos.pc
	@echo "== installed jaos $(JAOS_VERSION) under $(DESTDIR)$(PREFIX)"

uninstall:
	rm -f $(DESTDIR)$(INCLUDEDIR)/jaos.h
	rm -f $(DESTDIR)$(LIBDIR)/libjaos.a
	rm -f $(DESTDIR)$(LIBDIR)/libjaos.so
	rm -f $(DESTDIR)$(LIBDIR)/libjaos.so.$(JAOS_MAJOR)
	rm -f $(DESTDIR)$(LIBDIR)/libjaos.so.$(JAOS_VERSION)
	rm -f $(DESTDIR)$(BINDIR)/jaos
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/jaos.pc
	@echo "== removed jaos from $(DESTDIR)$(PREFIX)"
