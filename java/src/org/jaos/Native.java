// SPDX-License-Identifier: Apache-2.0
package org.jaos;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.invoke.MethodHandle;
import java.nio.file.Files;
import java.nio.file.Path;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BOOLEAN;
import static java.lang.foreign.ValueLayout.JAVA_DOUBLE;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static java.lang.foreign.ValueLayout.JAVA_LONG;

/** The C library's functions as downcall handles, one per function. */
final class Native {
    private Native() {}

    static final Linker LINKER = Linker.nativeLinker();
    private static final SymbolLookup LIB = load();

    private static SymbolLookup load() {
        String env = System.getenv("JAOS_LIBRARY");
        if (env != null && !env.isEmpty()) {
            Path p = Path.of(env);
            if (!Files.exists(p))
                throw new UnsatisfiedLinkError(
                    "JAOS_LIBRARY is set to '" + env + "', which does not exist");
            return SymbolLookup.libraryLookup(p, Arena.global());
        }
        String os = System.getProperty("os.name").toLowerCase();
        String file = os.contains("win") ? "jaos.dll"
                    : os.contains("mac") ? "libjaos.dylib" : "libjaos.so";
        for (Path dir = Path.of("").toAbsolutePath(); dir != null;
             dir = dir.getParent()) {
            Path candidate = dir.resolve("build").resolve("release").resolve(file);
            if (Files.exists(candidate))
                return SymbolLookup.libraryLookup(candidate, Arena.global());
        }
        return SymbolLookup.libraryLookup(file, Arena.global());
    }

    private static MethodHandle fn(String name, MemoryLayout ret,
                                   MemoryLayout... args) {
        MemorySegment at = LIB.find(name).orElseThrow(
            () -> new UnsatisfiedLinkError("libjaos has no " + name));
        FunctionDescriptor fd = ret == null ? FunctionDescriptor.ofVoid(args)
                                            : FunctionDescriptor.of(ret, args);
        return LINKER.downcallHandle(at, fd);
    }

    static final FunctionDescriptor LOG_FN =
        FunctionDescriptor.ofVoid(ADDRESS, JAVA_INT, ADDRESS);

    static final MethodHandle VERSION = fn("jaos_version", ADDRESS);
    static final MethodHandle NUM_OPTIONS = fn("jaos_num_options", JAVA_LONG);
    static final MethodHandle OPTION_NAME = fn("jaos_option_name", ADDRESS, JAVA_LONG);
    static final MethodHandle MODEL_NEW = fn("jaos_model_new", JAVA_INT, ADDRESS);
    static final MethodHandle MODEL_FREE = fn("jaos_model_free", null, ADDRESS);
    static final MethodHandle MODEL_ERROR = fn("jaos_model_error", ADDRESS, ADDRESS);
    static final MethodHandle STATUS_STR = fn("jaos_solve_status_str", ADDRESS, JAVA_INT);
    static final MethodHandle LOAD_LP = fn("jaos_load_lp", JAVA_INT, ADDRESS,
        JAVA_LONG, JAVA_LONG, JAVA_INT, JAVA_DOUBLE, ADDRESS, ADDRESS, ADDRESS,
        ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle READ_MPS = fn("jaos_read_mps", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle READ_LP = fn("jaos_read_lp", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle READ_NL = fn("jaos_read_nl", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle READ_QPLIB = fn("jaos_read_qplib", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle READ_OSIL = fn("jaos_read_osil", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle READ_CBF = fn("jaos_read_cbf", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle WRITE_MPS = fn("jaos_write_mps", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle WRITE_LP = fn("jaos_write_lp", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle WRITE_SOLUTION = fn("jaos_write_solution", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle NUM_COL = fn("jaos_num_col", JAVA_LONG, ADDRESS);
    static final MethodHandle NUM_ROW = fn("jaos_num_row", JAVA_LONG, ADDRESS);
    static final MethodHandle NUM_CONES = fn("jaos_num_cones", JAVA_LONG, ADDRESS);
    static final MethodHandle HAS_INTEGER = fn("jaos_model_has_integer", JAVA_BOOLEAN, ADDRESS);
    static final MethodHandle SET_COL_INTEGER = fn("jaos_set_col_integer", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_BOOLEAN);
    static final MethodHandle SET_COL_BOUNDS = fn("jaos_set_col_bounds", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_DOUBLE, JAVA_DOUBLE);
    static final MethodHandle SET_ROW_BOUNDS = fn("jaos_set_row_bounds", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_DOUBLE, JAVA_DOUBLE);
    static final MethodHandle SET_COL_COST = fn("jaos_set_col_cost", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_DOUBLE);
    static final MethodHandle SET_COL_QUADRATIC = fn("jaos_set_col_quadratic", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_DOUBLE);
    static final MethodHandle SET_QUADRATIC = fn("jaos_set_quadratic", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SET_ROW_QUADRATIC = fn("jaos_set_row_quadratic", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle ADD_CONE = fn("jaos_add_cone", JAVA_INT, ADDRESS, JAVA_INT, JAVA_LONG, ADDRESS);
    static final MethodHandle CONE = fn("jaos_cone", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle CONE_DUAL = fn("jaos_cone_dual", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle SET_OPTION = fn("jaos_set_option", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle GET_OPTION = fn("jaos_get_option", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, JAVA_LONG);
    static final MethodHandle SET_TIME_LIMIT = fn("jaos_set_time_limit", JAVA_INT, ADDRESS, JAVA_DOUBLE);
    static final MethodHandle SET_WORK_LIMIT = fn("jaos_set_work_limit", JAVA_INT, ADDRESS, JAVA_LONG);
    static final MethodHandle SET_THREADS = fn("jaos_set_threads", JAVA_INT, ADDRESS, JAVA_LONG);
    static final MethodHandle SET_MIP_START = fn("jaos_set_mip_start", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle SET_LOG_CALLBACK = fn("jaos_set_log_callback", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SET_LOG_LEVEL = fn("jaos_set_log_level", JAVA_INT, ADDRESS, JAVA_INT);
    static final MethodHandle SOLVE = fn("jaos_solve", JAVA_INT, ADDRESS);
    static final MethodHandle STATUS_OF = fn("jaos_status_of", JAVA_INT, ADDRESS);
    static final MethodHandle OBJECTIVE = fn("jaos_objective", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle SOLUTION = fn("jaos_solution", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle CERTIFICATE = fn("jaos_certificate", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle UNBOUNDED_RAY = fn("jaos_unbounded_ray", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle MIP_RESULT = fn("jaos_mip_result", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle MIP_INCUMBENT = fn("jaos_mip_incumbent", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle ITERATIONS = fn("jaos_iterations", JAVA_LONG, ADDRESS);
    static final MethodHandle WORK_UNITS = fn("jaos_work_units", JAVA_LONG, ADDRESS);

    static String string(MemorySegment p) {
        return p.equals(MemorySegment.NULL) ? ""
                                            : p.reinterpret(Long.MAX_VALUE).getString(0);
    }
}
