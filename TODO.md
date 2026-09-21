# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

Three milestones, in this order. A row in a later milestone waits for the
earlier ones unless it says otherwise. Every row was taken from the reading
of 2026-09-21 (the three audits of the docs, the code and the bench record)
and nothing from that reading is left out of this file. Each row says how
to verify it, so a row leaves when its check passes, not before.

## Milestone A: publish 0.4.0

The solver is correct and nobody can install it. A is what makes it
installable and makes every document true. Order inside A: A1 clears the
tree, A2 makes the docs true, A3 makes the library shippable, A4 cuts the
tag. A2 and A3 can interleave; A4 waits for both.

### A1. The working tree

A1.3 **The comment rule and the tree disagree.** `CLAUDE.md` says code
    carries no comments. `src/mip.c` (32 lines), `src/barrier.c` (28),
    `src/model.c` (15, the `quadratic_convex` block at 764), `src/simplex.c`
    (14), `src/conic.c` (12), `src/symmetry.c` (9), `src/check.c` (6) and
    `include/jaos.h` (8, the `jaos_set_mip_tree_batch` paragraph at 327)
    carry about 120 comment lines, all prose on an invariant. Fix: the rule
    stays; delete every block, and where the block states an invariant the
    code cannot, put the sentence in the commit message that removes it and,
    if it names a constant, in `docs/tolerances.md`; the header's paragraph
    goes to `docs/api.md` (A3.4). Verify: `grep -rn '/\*\|//' src cli
    include | grep -v SPDX` prints nothing.

A1.5 **The CBLIB gate cannot pass as written.** `bench/results/cblib.txt`
    reads 29 solved, 29 checker ok, 21 objective ok, "gate NOT MET", and
    there is no `bench/cblib.baseline`. The 8 objective misses are the
    `nql*` and `qssp*` files, whose library references are not optima (the
    checker certifies JAOS's lower value). Fix: in `bench/cblib.manifest`
    give those 8 the certified value with a note naming the reading that
    certified it, run `make cblib`, then `make cblib-baseline` after reading
    the diff. Verify: `make cblib` prints "gate: PASS" and
    "baseline: 0 regressed".

A1.6 **Values that overflow a double, three places.** Found on
    2026-09-21 while moving `test_solution_refuses_a_value_no_file_can_carry`
    to the batch's new contract. The model: two columns fixed at 1e300 in a
    free row with coefficients 1e10.
    - `src/presolve.c:340` asserts `isfinite(row_traffic[i])`, and the next
      line handles a traffic that is not finite. The model is legal input
      and a debug build aborts on it. Drop the assertion.
    - A release build ends `OPTIMAL` with the row activity `-nan`.
      `ps_row_add` computes `inf - inf` as its correction as soon as one
      term overflows. The batch ends a solve `NUMERICAL_ERROR` when the
      objective is not finite; the same rule goes for every published
      value (point, activities, duals, reduced costs), checked once where
      `jm_model_publish_objective` runs.
    - The checker recomputes each activity from the point. A row whose
      activity is `inf - inf` gives a NaN violation, and `max2` in
      `src/check.c` drops a NaN when a later row has a violation, so an
      infeasible point can pass. A NaN violation must count as infinite.
    Verify: a test per case (the model solves without aborting and ends
    `NUMERICAL_ERROR`; the checker refuses a point whose first of two rows
    overflows), and the rule written in `docs/api.md` with A3.4.

### A2. Every document true

Each row names the file and the lines as of dc7acc7. Verify each by
re-reading the named lines against the source the row cites, and at the
end of A2 by the check in A2.12.

A2.1 **`docs/feature-matrix.md`.** The stale cells and paragraphs:
    - `:29` says JAOS's column was last checked on 2026-09-08; cells carry
      dates to 2026-09-20. Do a real pass over `src/` and `cli/` and write
      its date.
    - `:46` MILP ◐; SPECS says done. Set ●.
    - `:72` "6 of the 94" past 10x on the primal; the record says 5
      (d6cube, dfl001, fit1d, fit2d, seba).
    - `:119-120` Maros-Meszaros "134 OPTIMAL, 133 checker"; SPECS says 137
      and 136.
    - `:144-145` and `:152-153` CBLIB "26 OPTIMAL, three sched_*_orig
      fail"; the record says 29 of 29 since 02-273. Name the real gap
      instead: QPLIB QCQP duals refused on 8 of 10, mixed-integer QCQPs at
      the work limit, `CONIC_QC_DENSE`.
    - `:197-208` presolve paragraph cites "phase 2", D97 and "TODO §1 and
      §3"; those rows are Windows and the primal. Point at
      `bench/refusals.txt` as SPECS does.
    - `:268-270` "this whole section [MIP] is out of scope and not
      scheduled". It shipped. Delete.
    - `:382-390` says the proof "is not written to a file"; `jaos_write_proof`
      exists and `:356` already says ●. Delete `:382-390`.
    - `:427-430` and `:628-630` say exact rational verification is partial;
      SPECS says done. Fix both.
    - `:437` Read LP ◐; SPECS says done. Set ●.
    - `:458-470` "104 of 139 through LP, 1 for a free row" is superseded by
      `:478-486`. Drop the old count.
    - `:555` and `:566-569` "Choose the algorithm ○ by decision D64";
      `--algorithm dual|primal|barrier|pdlp|concurrent` exists. Set ● and
      delete the D64 paragraph.
    - `:590-594` "absent from parallelism entirely"; section 5 reads ◐ ○ ●.
      Fix the sentence.
    - `:642-666` "the current milestone will barely move this page", "what
      would move the most cells: Python bindings, ranging", "Read LP and
      Write LP still ◐". All landed. Delete or rewrite.
    - `:313`, `:315-316`, `:446-449`, `:537-543`: rivals' `?` cells "due at
      the next pass", no pass since 2026-09-04. Do the pass over the rivals'
      public documentation, fill the cells, write the date at `:29`.
    - Feasibility relaxation reads ● while SPECS marks the row partial (a
      runaway case). Set ◐ and name the case, so every cell matches SPECS.
    - `:33` version line, with A4.1.

A2.2 **`README.md`.**
    - `:52-56` names only the dual simplex. Add one sentence with the five
      `--algorithm` values.
    - `:3-4` says JAOS solves SOCP and QCP "with integer columns or without"
      as if done; SPECS marks QP, QCP and MIQP partial. Change `:3-4` itself
      to say they are partial with a pointer to SPECS, and add a short
      "Quadratic and conic" paragraph after `:43-50`: barrier and push for
      QP, the conic interior point and its own tree for cones, and what is
      still partial.
    - `:58-66` MIP paragraph omits clique cuts and `--tree-batch`. Add.
    - `:84-86` command list omits `options`. Add.
    - `:137-138` "cross-compile for Windows"; the tool and the Python
      binding run natively since 2026-09-19. Say so.
    - `:158-159` names `make maros-meszaros` and not `make cblib`. Add.
    - `:196-202` the 3.60x / 1.12x / 2.96x reading is of tree bfde2d4,
      2026-08-30. Replace with B1's reading before A4.
    - `:17` version, with A4.1.
    - Add a `CONTRIBUTING.md` (how to build, `make test && make sanitize`,
      the three record files, the rules from `CLAUDE.md` that a contributor
      must hold: bit-identical, no dependencies, no code from other
      solvers, constants in `docs/tolerances.md`) and a `SECURITY.md` (where
      to report, the readers are the attack surface), and link both from
      README's document list (the block near `:222-226`; check the line).
    - There is no `docs/README.md` or `docs/index.md`; README's document
      list is the only map of `docs/`. Add `docs/README.md` with one line
      per file (build, cli, api, format-support, feature-matrix, scaling,
      tolerances, work-units, research/) and keep README's list pointing at
      it. `tools/docs-check.sh` (A2.12) checks every file in `docs/` has a
      line there.

A2.3 **`docs/cli.md`.**
    - `:14-42` usage block lacks `--write-duals`, `--tree-batch`, `--opt`,
      `--params`, the `options` command and `STUB -AMPL`. Copy the block
      from `cli/jaos.c:25-76` and keep them in step.
    - `:3-4` "four analyses"; `relax` is the fifth.
    - `:224` `--algorithm`: names three, then describes five. Say five.
    - `:226` `--opt` list has 53 names; `jaos options` prints 54.
      `mip_tree_batch` is missing.
    - `:329-330` "104 to 138, the one left is greenbea's free row"; free
      rows are written since 2026-09-19. Say 139 or re-measure 02-219.
    - `:846`, `:884`, `:1083`, `:1087-1088` give a refused proof exit 4, 3,
      3 and "4 is solve's alone" while `check --proof` exits 4 at `:528`.
      Read `cli/jaos.c:19` (`EXIT_NUMERICAL = 4`) and every `return`/`exit`
      in the verify, check and solve paths, then make the four places agree.
    - `:1078-1084` exit-code table lacks `relax` (0/5 at `:779-782`),
      `convert`, `stats`, `diff`, `show`, `options`. Add every command.

A2.4 **`docs/format-support.md`.**
    - `:3-4` "the three writers"; seven model writers.
    - `:11`, `:14-15` "both readers take gzip"; six readers.
    - `:508` "both formats ... both readers"; fix.
    - `:493-499` writers list omits `jaos_write_point`, `jaos_write_proof`,
      `jaos_write_mps_basis`, `jaos_write_sol_ampl`. Add.
    - `:595-599` "104 of 139 round-trip through LP, 1 for a free row";
      superseded by `:571-575`. Fix the count or date the reading.
    - `:789-790` "the two model writers"; six.

A2.5 **`docs/work-units.md`.**
    - `:44-45` "everything is in lu.c, chol.c, presolve.c, simplex.c;
      nothing else counts". `barrier.c`, `pdlp.c`, `conic.c`, `conictree.c`,
      `concurrent.c`, `mip.c`, `symmetry.c` and the certificate re-weighting
      (`CONIC_CERT_CALLS`) all bill. Read each kernel's billing and write
      one section per kernel with its charges, as the simplex has.
    - `:10-18` "a budget that stops keeps the basis; raising the limit
      continues" predates the bit-exact parked resume of 2026-09-15. Update.
      Verify the barrier claim at `:16-18` against `src/barrier.c` and
      correct it.
    - `:52-53` "the three committed baselines"; `bench/` holds twelve.
    - The attribution table is of D32 and the doc says the next run
      replaces it. B4 replaces it; until then date it.

A2.6 **`docs/scaling.md` `:86-97`** describes a geometric-mean mode as an
    option and an open question. SPECS says Curtis-Reid, done; the mode is
    `JM_SCALE_GEOMETRIC` inside `src/scale.c` with no API. Say the default
    is decided and the pass is internal, or delete the section.

A2.7 **`docs/tolerances.md`.** `:426` names `CHECK_TOL` and `:816` names
    `CONIC_NEWTON_DENSE`; neither exists in the source (the second is
    `CONIC_NEWTON_WIDE`). Fix the names. `:557` heading "What is not
    settled" over a section that settles everything; rename.

A2.8 **`bench/README.md` `:47-57`** lists `warm`, `primal`, `barrier` as the
    other runners. The Makefile also has `barrier-infeas`, `pdlp`,
    `pdlp-infeas`, `concurrent`, `primal-kennington`, `plato`, `cblib`,
    `maros-meszaros`. List every runner whose result sits in `bench/results/`.

A2.9 **`SPECS.md`.**
    - `:132` "183 functions"; the header declares 197 or 198 (the two
      audits of 2026-09-21 read different counts). Count by grep on
      `include/jaos.h` first, then write that number here and in A3.2,
      A3.4 and A3.5.
    - `:153` "42 options"; `jaos options` prints 54.
    - `:152` "`--algorithm dual|primal|barrier`"; five values.
    - `:89` "20 of the MIP set's 23"; the set is 24 and the refusal says
      all but l152lav ran. Say 23 of the 24.
    - `:4` "a linear and mixed-integer programming solver"; README says LP,
      MIP, QP, SOCP, QCP. Align.
    - "The bars": MIPLIB 2017 "not started" changes when B7 lands.

A2.10 **This file.** Rows C4 to C7 say "SPECS row 22 / 23 / 24 / 82". Those
    are line numbers and any new SPECS line breaks them. Name the row
    ("SPECS §1, Convex quadratic (QP)"). C3's sentence "d6cube and degen3
    stand still" puts degen3 among the five overruns; degen3 is at 4.0x,
    under the bar. Say it is a stall example under the bar.

A2.11 **The time limit and determinism.** `jaos_set_time_limit` reads
    `clock_gettime` (`src/sys.c:220`) and a run that stops on it is not
    bit-identical across machines. Say so in `docs/cli.md` at `--time-limit`
    and in `docs/api.md` (A3.4), if not already said.

A2.12 **The check that closes A2.** Re-run the 2026-09-21 docs audit's
    method on the tree: every `--flag` in `cli/jaos.c` is in `docs/cli.md`
    and back; every reader and writer in `src/` is in `format-support.md`;
    every constant named in SPECS, cli.md and work-units.md is in
    `tolerances.md` and exists in the source; every `bench/measurements/`
    directory named in SPECS, TODO, README and docs exists; every count in
    SPECS (functions, options, instances) matches `grep`. Put the method in
    `tools/docs-check.sh` so it runs from `make test`, and it prints nothing
    when the docs are true. This row leaves when the script exists, runs in
    `make test` and passes.

### A3. Shippable

A3.1 **CI.** There is no `.github/` and no other CI. Add
    `.github/workflows/ci.yml`: an Ubuntu job with GCC 14 that runs `make
    test && make sanitize && make python-test` (the shared library first),
    and a second job with mingw-w64 and wine that runs `tests/windows.sh`.
    Verify: the workflow file parses (`act` is not required; a push to a
    branch and a green run is the check, and it is the user's to trigger).

A3.2 **Symbol visibility.** All 91 non-static `jm_*` functions in
    `src/jaos_internal.h` export from `libjaos.so` and from the mingw DLL.
    Add a `JAOS_API` macro in `include/jaos.h` (`__attribute__((visibility
    ("default")))` on GCC and clang, `__declspec(dllexport)`/`dllimport` on
    Windows), mark the 197 public functions, build with
    `-fvisibility=hidden` in both `Makefile` and `CMakeLists.txt`. Verify:
    `nm -D build/release/libjaos.so | grep ' T ' | grep -v jaos_` prints
    nothing, and `tests/windows.sh` still runs.

A3.3 **The shared library's identity.** `Makefile:58` links `libjaos.so`
    with no soname; `CMakeLists.txt:123` sets `SOVERSION 0`. Give the
    Makefile the same soname (`libjaos.so.0`, with the unversioned symlink)
    so `make install` and the CMake install agree. Verify:
    `readelf -d build/release/libjaos.so | grep SONAME` and
    `tests/install.sh`.

A3.4 **API reference.** 197 public functions and no document lists them.
    Write `docs/api.md`: every function grouped as `jaos.h` groups them, one
    line each with what it does, its status return and what it fills; the
    ownership rules (`jaos_model_new`/`free`/`copy`, strings owned by the
    model); that `jaos_get_option` is the read path for every `jaos_set_*`
    and each setter's option name; the `jaos_set_mip_tree_batch` paragraph
    from the header; the time-limit note of A2.11. Link from README's
    document list and from `docs/README.md` (A2.2).
    Verify: a script step in `tools/docs-check.sh` that every `jaos_*` in the
    header appears in `docs/api.md`.

A3.5 **The header, before 1.0.** 34 enumerators carry no explicit value and
    `include/jaos.h:819` is an anonymous `typedef enum` (`jaos_proof_kind`).
    Give every enumerator its value and the enum its tag. Verify: the value
    of every enumerator is unchanged (compare `jaos options` and the Python
    tests, which read them by number).

A3.6 **`const` on read-only calls.** `jaos_row_entries` (`jaos.h:405`),
    every `jaos_write_*` (`:435-449`) and `jaos_write_proof` take a non-const
    model, so a `const jaos_model *` cannot be written out. Make them take
    `const`, or write in `docs/api.md` why each mutates. Verify: builds with
    `-Wcast-qual` clean, Python and bindings unchanged.

A3.7 **pip from source.** `pyproject.toml` has no `MANIFEST.in` and no sdist
    config, so an sdist carries no `src/`, `include/`, `cli/` or `Makefile`
    and `pip install jaos` from PyPI cannot build. Add `MANIFEST.in` (`graft
    src include cli`, `include Makefile CMakeLists.txt`) or the setuptools
    sdist table. Verify in WSL, in a clean venv: `python -m build --sdist &&
    pip install dist/*.tar.gz && python -c "import jaos; print(jaos.version())"`.
    Put that check in `tests/install.sh` or a `make sdist-test`.

A3.8 **Wheels.** No `cibuildwheel` config, no manylinux, no macOS or Windows
    wheel; the loader in `python/jaos/__init__.py:186-224` already looks for
    `jaos.dll` and `libjaos.dylib`. Add a `cibuildwheel` job to A3.1's
    workflow for manylinux x86_64 at least, and a Windows wheel from the
    mingw DLL if the mingw job can produce it. Verify: the workflow builds a
    wheel that installs in a clean venv (the user's push is the run).

A3.9 **Version in five places by hand.** `include/jaos.h:22-25`,
    `pyproject.toml:7`, `julia/JAOS/Project.toml:4`, `R/jaos/DESCRIPTION:4`,
    `dotnet/Jaos/Jaos.csproj:9`, plus README `:17` and feature-matrix `:33`.
    Add `make version-check` that greps each against the header and fails
    on a mismatch, and run it from `make test`. Verify: change one, see it
    fail, restore.

A3.10 **The other bindings' metadata.**
    - `julia/JAOS/Project.toml`: `authors = ["JAOS contributors"]` is a
      placeholder; no JLL, so document how the package finds `libjaos.so`
      (`Libdl` lookup and the `JAOS_LIBRARY` variable) and check it works
      from a fresh Julia depot.
    - `R/jaos/DESCRIPTION`: `Maintainer: JAOS contributors <jaos@invalid>`
      is rejected by CRAN. Put the user's name and address (ask; do not
      invent one).
    - `dotnet/Jaos/Jaos.csproj`: no `PackageId`, `Authors`, `RepositoryUrl`,
      and no `runtimes/linux-x64/native/libjaos.so`, so a NuGet package would
      ship managed code only. Add them and the native asset from the build.
    - `java/`: no `pom.xml` or `build.gradle`, no version, no coordinates.
      Add a minimal `pom.xml` that compiles `java/src/org/jaos/*.java` and
      runs `java/check/Check.java` against `build/release/libjaos.so`.
    Verify: `make julia-test r-test dotnet-test java-test` (whatever the
    Makefile names them) pass.

A3.11 **Fuzz target for the readers.** `tests/test_fuzz.c` is a seeded model
    generator; nothing feeds bytes to `jaos_read_mps`, `jaos_read_lp`,
    `jaos_read_nl`, `jaos_read_osil`, `jaos_read_qplib`, `jaos_read_cbf`.
    Add `tests/fuzz_readers.c` with `LLVMFuzzerTestOneInput` that writes the
    input to a scratch file and calls each reader, a `make fuzz` target
    under clang with `-fsanitize=fuzzer,address,undefined`, and a corpus
    seeded from `tests/data/`. Run each reader for ten minutes once and fix
    what it finds. Verify: `make fuzz` builds; the run's findings are fixed
    with a test each.

A3.12 **Small robustness rows.** `src/inflate.c:441` keeps a 64 KB chunk on
    the stack (a Windows worker thread has 1 MB; move it to the heap or note
    the bound). `src/model.c:757` truncates a log line at 1024 bytes
    silently (say so in `docs/api.md` or grow it). `cli/jaos.c:1719, 3203`
    hard-code a 4096-byte path (use `PATH_MAX` from `jaos_sys.h` or
    allocate). 285 direct `malloc/calloc/realloc` sites bypass
    `jm_alloc_array`'s overflow check; grep each for a size that is a
    product and route those through `jm_alloc_array`.

A3.13 **Valgrind and coverage: neither exists.** ASan under `make sanitize`
    is the only memory check and nothing measures which lines the 846 unit
    tests reach. Add `make coverage` (`--coverage`, `gcov` or `lcov`, the
    summary printed per `src/` file) and `make valgrind` (the unit suite
    under `valgrind --error-exitcode=1`, one run; it is slow, so not in
    `make test`). Run each once, fix what they find, and put the coverage
    figure per file in `CONTRIBUTING.md` so a contributor knows where tests
    are thin. Verify: both targets run in WSL and exit 0.

A3.14 **`python/jaos/__init__.py` is one 4083-line file.** Split into
    `_native.py` (ctypes), `model.py` (the modelling layer) and
    `reports.py`, with `__init__.py` re-exporting so nothing a user imports
    changes. `src/write.c` (3206 lines, every writer) may split per format
    the same way. Verify: `make python-test`, `make test`. Last in A3; it
    blocks nothing.

### A4. The tag

A4.1 **Cut v0.4.0.** After A1 to A3 and B1: bump the seven version places
    (A3.9 lists them), `make test && make sanitize && make python-test`,
    the four gates and `make miplib` (solver internals moved in A3.2's
    build flags and in B1's re-take), commit, tag `v0.4.0`, push from
    Windows. The tag message lists what landed since 0.3.0 by SPECS row:
    cones and CBF, QCP, the conic tree and `--tree-batch`, Julia, .NET,
    Java, R, the AMPL protocol, native Windows, the concurrent solve, PDLP,
    the resume, the proof file. Verify: `git describe` says `v0.4.0`;
    `pip install` of the sdist reports 0.4.0.

## Milestone B: performance

The gap against HiGHS is 3.60x per solve (P0, tree bfde2d4, 2026-08-30):
1.5x to 2.0x per iteration on every instance, and the iteration count on
four instances. Rows B1 to B9 in gain order; B10 and B11 are unmeasured
components with no expected gain on record, so they come after. Each is unrefused today; read the named
refusal before starting and stop if its condition is not met. Every row
that changes `simplex.c`, `lu.c`, `presolve.c`, `scale.c` or `mip.c` runs
the gates (`CLAUDE.md`, step 3). B1 is part of A4's gate; the rest follow
the tag.

B1 **Re-take P0.** 58 solver-internal commits since bfde2d4 and no reading.
    `make compare COMPARE_ARGS='-t P0'` (the three rivals are built in
    `bench/compare/solvers/`). Write the new table into `README.md:196-202`,
    `bench/compare/README.md:88-97` and `docs/feature-matrix.md` where it
    cites the ratios. Every later B row reads its before from this file.
    Verify: `bench/compare/results/P0.txt` names today's tree.

B2 **Repair drifted DSE weights instead of restarting them all.** pilot87
    runs 12.4x and pilot 10.6x HiGHS on P0 (take the per-instance ratios
    from B1's fresh `P0.txt`, not from here), and pilot, pilot87, 25fv47
    and greenbea all restart their weights on 80 to 93% of iterations
    (D63). With the restart off the iterations fall to 0.31x to 0.54x, and
    `DSE_DRIFT` at 2.0 gives a false INFEASIBLE on greenbea, at 100 grow22
    7.2x. D63 refused moving the threshold only. Try: recompute the weight
    that drifted and keep the rest; or fall back to Devex weights for the
    drifted rows until the next refactorization. pilot87 alone is 56% of
    the netlib gate's work. Bar: the gates' 2.0x per instance, geometric
    mean under 0.95x over the 94, no verdict changes. If refused, one line
    in `bench/refusals.txt` with the reopen condition. If the Devex
    fallback lands, the `SPECS-crash-basis` refusal reopens ("Devex
    landing" is its condition): re-measure the crash basis then, in the
    same batch or the next.

B3 **Aggregator, doubleton-equation substitution in presolve.** stocfor3
    is the worst instance against HiGHS (27.4x) and Clp (22.8x); 02-20 says
    the gap is the aggregator, and 02-10 counts 28% of Kennington rows as
    doubletons. The mechanism needs bound transfer, which D97 refused six
    designs of on false INFEASIBLE. D97 reopens on a crossover at postsolve;
    D114 met its first precondition and `crash_basis` in `src/barrier.c`
    now exists. Build the substitution with the postsolve that restores the
    bounds and the basis, and measure over netlib and kennington. Same bar
    as B2. Read D97 in `git show 2d3c56b:DECISIONS.md` first.

B4 **Fresh attribution of the iteration, then D93's scan.** Per iteration
    JAOS costs 1.5x to 2.0x every rival. The attribution in
    `docs/work-units.md` is of D32 and predates D40, D41 and D93. Run
    `tools/icount.sh` per function over truss, fit2d, pilot87 and maros-r7,
    write the table into `docs/work-units.md` (A2.5's dated table goes),
    then take the largest share. D93's dense candidate scan of the ratio
    test is 15% of instructions on truss and its 4.2% bar is readable now
    with `tools/icount.sh`. Bar: instructions down by more than the 0.3%
    noise on the LU-heavy and the pricing-heavy instance, work units not
    up, answers byte-identical.

B5 **grow22's presolve firings.** Presolve makes grow22 11.16x more
    expensive from 20 singleton-column firings (02-11); the primal solves it
    at 0.0385x the dual's work. D112 refused a widening rule and D108/D109
    the window floor; no line refuses a rule that reads a firing's effect on
    the basis (the fill or the condition of the columns it leaves). Read
    02-11 and D108 to D112 first. Same bar as B2, and grow22 under 2x its
    baseline work is the point of the row.

B6 **The crossover push.** SPECS's crossover row is missing a primal and
    a dual push; from the barrier's point the ranked guess costs more than
    a cold dual solve on 21 of 94 (d2q06c 237232 iterations against 27935,
    pilot87 83342 against 37362). `crossover-primal`, `crossover-dual-slack-key`
    and `crossover-tight-barrier` are refused; the push itself is not.
    Build the push (Bixby and Saltzman's form: move each nonbasic column to
    a bound along a direction that keeps primal feasibility, then the dual
    push), measure `make barrier`. Bar: fewer than 21 overruns and the
    barrier's geometric mean under 2.705x the dual. SPECS row "Crossover"
    changes to done when the count is zero.

B7 **The MIPLIB 2017 reading.** The MIP tree has only ever been measured
    on the 24 MIPLIB 3 instances (18 to 10757 columns). Eight standard
    components sit off behind switches after readings of 0.97x to 1.18x on
    that set, and node dives (D289) were refused and not kept. No rival's
    node count or time is in the record for any MIP. Add
    `bench/instances-miplib2017/` with the easy subset (the manifest with
    checksums as the others have), a `make miplib2017` runner with a
    reference objective per instance, a work limit per instance, and a
    first reading into `bench/results/miplib2017.txt` and a
    `bench/measurements/` directory. Extend `bench/compare` to the MIP set
    the way it reads LP: HiGHS and SCIP (or CBC) beside JAOS on MIPLIB 3
    and the 2017 subset, seconds and node counts, in
    `bench/compare/results/`, so the MIP gap is a number and not a guess.
    Then re-measure the switched-off components on 2017, one at a time:
    reliability branching (D293), bound propagation (D324), reduced-cost
    fixing (D323), root probing, flow cover, zero-half, lifted cover
    (D307), RINS (D315), node dives (D289). Each that pays there without
    breaking MIPLIB 3's bar lands on; each that does not gets its refusal
    line extended with the 2017 number. SPECS "The bars" then says what the
    reading is.

B8 **Parallel tree for `src/mip.c`.** Row C7 below has the design. After
    B7, because a wall-clock reading needs the larger set.

B9 **A parallel simplex or a parallel barrier.** SPECS's "Parallel LP"
    row: one method on N cores on one factorisation. Absent and not
    refused. The barrier's normal-equation Cholesky is the natural first
    (parallel column blocks in `src/chol.c` with a fixed schedule, so the
    result is bit-identical). Last in B; it needs a design reading first.

B10 **Cost perturbation from the start.** The dual perturbs costs on the
    first stall and never before (SPECS "Dual simplex"). Every rival
    perturbs from the first iteration on a degenerate model. The record has
    no reading and no refusal either way. Measure it over netlib and
    kennington under B2's bar. One line in `bench/refusals.txt` if it loses.

B11 **Local branching, MIP restarts, node selection.** SPECS "RINS, local
    branching" is partial: RINS is off by measurement and local branching
    was never built. Build it behind a switch and measure on B7's set. MIP
    restarts (a root restart after enough fixings) and a node selection
    beyond best-bound (best-estimate, or a plunge with a bound gap) have no
    SPECS row and no refusal; SPECS is closed, so the user decides whether
    to add the two rows. Ask once, with B7's numbers, before building.

## Milestone C: reach and polish

The research rows. They were here before 2026-09-21 and stay as written.
C3 is B's item 9 (the primal's five instances). C7 is B8.

C1 **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and since 2026-09-19 `tests/windows.sh` runs the tool and the Python
   binding natively on the Windows host WSL runs on (02-251). Missing:
   clang-cl, which needs Microsoft's C runtime headers and libraries,
   licensed by Microsoft and not on this machine, and macOS, which needs a
   Mac.

C3 **Primal simplex: 5 of the 94 standard instances run past 10x the dual's
   work** (`bench/results/primal.txt`): d6cube, dfl001, fit1d, fit2d, seba.
   None disagrees. The SPECS row stays partial until the count is zero.

   **Do not re-measure these.** Six ideas are refused with their reopen
   conditions in `bench/refusals.txt`: `primal-bound-perturbation`,
   `primal-tie-hash`, `primal-cost-perturbation`, `primal-noise-floor`,
   `primal-expand-step`, `primal-expand-schedule`. Steepest edge, Devex and Dantzig are all already
   read over the set; the SPECS row records them. Cost per iteration was
   read and paid 7.6% over the set for nothing here, because the gap is the
   iteration count.

   **The five are two faults, not one**, so a remedy aimed at either reads
   as noise over the set unless it is measured on its own group (d5a43e9).

   - d6cube and degen3 stand still. d6cube holds one vertex for 8508
     consecutive phase-2 bases. They need a remedy that leaves a vertex.
     EXPAND is now measured whole (`primal-expand-schedule`, 2026-09-15):
     the step, the growing width, the reset and the hold at the ceiling
     get d6cube to 0.33x at best and 27x the dual's work, and the walk is
     chaotic in the width, so no schedule closes the gap. What is left to
     try on d6cube is the pricing side, which is also what seba and fit1d
     point at.
   - seba and fit1d do not stall at all. They leave for a new point at
     nearly every pivot, so degeneracy is not what holds them and the
     target is the entering column. What holds them is still unnamed.

   The walk revisits no basis, so there is no cycle for an anti-cycling
   rule to break, which is why Bland's rule never pays here (f954aee).

C4 **Convex QP: the Maros-Meszaros set.** SPECS row 22. `make
   maros-meszaros` reads the 138 instances against BPMPD's values; the
   first reading (`bench/measurements/02-250/`) found and 808022a fixed
   the NaN points, the 1e-6 row slips, the four unreadable files and the
   handoff that ground the dual simplex; six more fixes followed from its
   list (02-250, "After the reading"), and the push's stretch and its
   longer freeing took two more on 2026-09-19, the augmented restart took
   boyd2, and the conic interior point after a failed barrier took dtoc3,
   ksip and ubh1 (`bench/measurements/02-258/`; the barrier alone still
   stops on the three). What it leaves, 132 of 138 clean, 137 `OPTIMAL`
   and 136 taken by the checker:

   - **one the checker refuses with the objective right**: qgrow22. Its
     first push step came out NaN, and since 2026-09-19 the push takes
     such a round again on a larger regularisation; it then runs 40
     rounds and ends with 19 pinned variables of the wrong sign, the worst
     2.4e5 against 2.8e-9. The barrier's own duals are off by 3e-6.
     qsierra and qgfrdxpn were here until the push learned to stretch a
     stalled step along the rows' null space (`QP_PUSH_EXTRAPOLATE`) and
     to go on freeing while the wrong signs fall. liswet10 and liswet11
     were here until the walk learned to go on to `BARRIER_TOL_QP` when
     the push does not settle; qsierra and qgrow22 cannot reach it.
     qisrael, qpilotno and boyd1 were the same list until the push
     learned to release a pin in a row it left unsatisfied and the
     checker's row test went relative to the row's traffic.
   - **values is refused as not convex**, and it is not: its `Q` has 60
     eigenvalues below zero, down to -1.27e-5 against a largest of 10.77,
     the six-digit rounding of a covariance. BPMPD's 1.3966211 is a
     stationary point. Nothing to fix unless the contract changes to take
     a `Q` within its data's precision of semi-definite.
   - **hues-mod and liswet2** end at the checker's optimum, certified, but
     6e-6 and 1e-6 away from BPMPD's value.
   - **aug2dcqp, aug2dqp, aug3dqp** pass the checker but not the runner's
     suboptimality ceiling: `Σ d_j (x_j - l_j)` over columns of 1e6 with
     reduced costs of 1e-9.
   - **QPLIB's convex QPs** (`bench/measurements/02-256/`): 10 of 19 end
     `OPTIMAL` within 5.7e-7 of the library's values. QPLIB_9002 ends
     `OPTIMAL` on the barrier's own test with its rows 8.9e-7 off and a
     dual violation of 2.1e4, the push leaving 931 pinned columns with
     the wrong sign. The 8 largest (10000 to 1003001 columns) reach a
     work limit of 1e11, and QPLIB_9008 (1009306 columns) runs out of
     memory. Their normal equations filled badly, and since 2026-09-20
     the barrier reads both factors' operation counts and keeps the
     cheaper (`bench/measurements/02-272/`), so QPLIB_8785 reaches 59
     iterations in the budget where it reached 7, QPLIB_10038 16 where
     it reached 8 and QPLIB_10034 169 where it reached 84. None of them
     finishes: on the augmented system QPLIB_8785 reaches the library's
     objective by iteration 39 and its dual residual then shrinks by a
     quarter per iteration with `mu` at 1e-40, and QPLIB_10034 does not
     converge in 169 iterations. What is left is the walk itself, not
     the system it factors.


C5 **Cones and quadratic rows, the rest.** SPECS row 23. The conic
   interior point, its Newton finish and every format with CBF landed on
   2026-09-19 (`bench/measurements/02-253/`), and the CBLIB reading the
   same day (`make cblib`, `bench/measurements/02-254/`). Missing:

   - **the conic tree's reach** (`src/conictree.c`, since 2026-09-19,
     `bench/measurements/02-255/`). It branches and plunges, and rounds and
     dives at the root, with no cuts and no warm start, so 37 of CBLIB's
     80 mixed-integer instances end at the work limit of 1e11
     (`bench/measurements/02-263/`), each with an incumbent: the
     sssd-*-8, uflquad-nopsc and turbine07_lowb_aniso files 0.14 to 1.7
     above the reference, their bounds 5% to 65% below it. Pseudocost
     branching (02-261) closes most of the sssd bounds and solves 28 of
     the 80 at 1e10 against 26, and costs the robust_50 files 2x to 4x
     the work. The cones the walk now leaves out (02-262) and the
     certificates it trims (02-263) took turbine07, turbine54 and
     turbine07_lowb to `OPTIMAL`. SOS sets, semi-continuous columns and
     indicator rows beside cones are refused.
   - **a certificate whose free column has to vanish exactly.** The
     checker caps a column by its curvature since 2026-09-20
     (`bench/measurements/02-267/`), and a refused certificate is
     re-weighted since the same day (`bench/measurements/02-270/`), so
     313 of 02-253's 314 ball-and-half-space models publish a
     certificate where 3 did. What the re-weighting cannot reach is a
     free column with no curvature: its coefficient `a` has to be 0
     exactly, which is one equation over the multipliers, and a search
     that moves one multiplier at a time cannot hold an equation. The
     answer is a solve over the multipliers, which is a second-order
     cone program: the gap is concave in them, `a²/(-2h) ≤ t` is a
     rotated cone in `(t, -h, a)`, a column with a finite bound gives
     two linear rows, and a free column with no curvature gives `a = 0`.
     An off-diagonal quadratic part is still refused by
     `row_curves_its_way`; it needs the supremum of a concave quadratic
     form, which is `a'H⁺a / 2` where the shift `u` of
     `a'x + ½x'Hx ≤ (a + Hu)'x - ½u'Hu` makes `a + Hu` vanish.
   - **CBLIB's twelve filterdesign instances** (71 to 872 MB) are not
     read at all. The three `sched_*_orig` are done since 2026-09-20
     (`bench/measurements/02-271/` and `02-273/`): the Newton finish's
     point is taken when the walk's own is refused and the finish's
     violation is smaller, and every column that leaves the finish on a
     bound with a reduced cost pushing it the other way leaves the
     active set, so `make cblib` reads 29 of 29 solved and taken by the
     checker where it read 26.
   - **QPLIB's convex QCQPs** (`bench/measurements/02-256/`). Of the 13
     continuous ones, 10 end `OPTIMAL` within 2.7e-7 of the library's
     values, but 8 of those have duals the checker refuses at 1e-7, off
     by 8.5e-7 to 3.6e-5 with the primal side to 2e-13. The Newton finish
     is refused on them: on QPLIB_2482, 1682 active rows over 1806 free
     columns leave directions with no curvature, and one step moves a
     column by 1.44 and breaks row 1759, which the walk left inactive.
     Taking rows within 1e-6 of a side as active and letting Newton run
     six steps reaches a KKT residual of 9e-16 on 1683 constraints, and
     the checker then finds a violation of 5.1e-3, so the missing piece
     is an active-set update after the finish. What the checker refuses
     is the rows, not the columns: on QPLIB_3088 the ball rows q(x) <= 1
     end 2e-6 to 1e-4 from their side with duals of 1e-7 to 2.5e-6, their
     products near 1e-11, while every reduced cost is under 3e-9; fitting
     each row's multiplier to its cone's whole dual and damping the
     finish both change nothing (`conic-qc-dual-refit`,
     `conic-newton-prox`); QPLIB_2676 and
     QPLIB_2468 stop without progress and end `NUMERICAL_ERROR`; QPLIB_3312
     (41406 columns) reaches the work limit. Of the 14 mixed-integer
     ones, the two `LMD` files end within 1e-2 of the reference at the
     work limit, 10 `LMC` files reach it, 8 of them with no incumbent,
     and QPLIB_10006 and 10007 are refused for a quadratic row over
     `CONIC_QC_DENSE` (3000) columns.
   - **a badly scaled box beside a cone that stays**: 15 bound-only
     columns of QPLIB_9002, values of 1e9 against `Q` entries of 4e-11,
     end the walk at a certificate the checker refuses ("columns reach
     inf"). With the box alone the model now leaves the walk, since its
     one cone is idle and nothing conic is left
     (`bench/measurements/02-266/`), and the barrier solves it at
     73622257.83. Beside a cone the walk cannot leave out
     (`tests/data/g_cone_badbox.mps`) the walk still fails, and that is
     the walk's own trouble with a box of that scale.

C6 **Mixed-integer quadratic, the QPLIB reading.** SPECS row 24. Of
   QPLIB's 17 convex mixed-integer QPs (`bench/measurements/02-256/`,
   `02-258/`, `02-259/`), 4 end `OPTIMAL` and 13 reach a work limit of
   1e11: QPLIB_3871, 3698, 3792, 3694 and 3861 with incumbents 27% to
   71% above the reference, QPLIB_3913, 4270 and 3547 with incumbents
   32.6%, 6.4% and 64% above it, and QPLIB_3980, 5577, 5924, 5527 and
   5543 with none. Each node is a cold barrier solve. The conic tree,
   measured on the same set and refused (`miqp-conic-tree`), ends the DML
   files 1.5% to 62% above the reference and finds QPLIB_3980 an
   incumbent at node 96; its root heuristics now run on the MIP tree too
   (02-259) and do not reach those, so the difference is in the search.
   QPLIB_5577, 5924, 5527 and 5543 (6014 to 25700 columns) spend the
   whole budget at the root node, and the last three never finish its
   relaxation.

C7 **Parallel tree search, the rest.** SPECS row 82. The conic tree takes
   its open nodes in rounds since 2026-09-20 (`--tree-batch N`,
   `bench/measurements/02-264/`): a round's relaxations solve on up to
   `--threads` threads and their answers are taken in the round's own
   order, so nothing the solve publishes depends on the thread count.
   Rounds are off by default, because a round of four leaves a worse
   incumbent where a work limit stops the tree. Missing: the tree of
   `src/mip.c`. Its nodes warm start from their parent's basis and share
   a cut pool, so a round there is not the round of cold walks the conic
   tree has; each worker would need its own copy of the basis and the
   pool, and the cuts a round finds would have to be taken in its order.
