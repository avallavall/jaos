# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

1. **Defect: `klein2` cycles warm from its own infeasible basis.** A period-two
   cycle between the ratio test's pivot floor and `LU_AGREE_TOL`. Fix so the
   warm attempt answers without the cold retry.
2. **Model statistics count the new kinds.** `jaos_model_stats` and
   `jaos stats` report semi-continuous columns, SOS sets and indicator rows.
3. **Bound propagation on the model's rows at the root, on by default only if
   it measures better** on the MIP set with the clique cuts in; the switch
   exists and is off.
4. **CMake package.** A `CMakeLists.txt` that builds the library, the tool and
   installs a config file, beside the Makefile.
5. **Windows build.** The library and the tool under MSVC or clang-cl, with the
   POSIX calls (`getline`, `newlocale`, `clock_gettime`) behind one shim.
