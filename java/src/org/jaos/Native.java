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

    private static MethodHandle setter(String name, MemoryLayout value) {
        return fn(name, JAVA_INT, ADDRESS, value);
    }

    private static MethodHandle withPath(String name) {
        return fn(name, JAVA_INT, ADDRESS, ADDRESS);
    }

    static final FunctionDescriptor LOG_FN =
        FunctionDescriptor.ofVoid(ADDRESS, JAVA_INT, ADDRESS);
    static final FunctionDescriptor EVENT_FN =
        FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS);

    static final MethodHandle VERSION = fn("jaos_version", ADDRESS);
    static final MethodHandle STATUS_NAME = fn("jaos_status_str", ADDRESS, JAVA_INT);
    static final MethodHandle STATUS_STR = fn("jaos_solve_status_str", ADDRESS, JAVA_INT);
    static final MethodHandle INFINITY = fn("jaos_infinity", JAVA_DOUBLE);
    static final MethodHandle NUM_OPTIONS = fn("jaos_num_options", JAVA_LONG);
    static final MethodHandle OPTION_NAME = fn("jaos_option_name", ADDRESS, JAVA_LONG);
    static final MethodHandle MODEL_NEW = fn("jaos_model_new", JAVA_INT, ADDRESS);
    static final MethodHandle MODEL_FREE = fn("jaos_model_free", null, ADDRESS);
    static final MethodHandle MODEL_ERROR = fn("jaos_model_error", ADDRESS, ADDRESS);
    static final MethodHandle MODEL_COPY = fn("jaos_model_copy", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle LOAD_LP = fn("jaos_load_lp", JAVA_INT, ADDRESS,
        JAVA_LONG, JAVA_LONG, JAVA_INT, JAVA_DOUBLE, ADDRESS, ADDRESS, ADDRESS,
        ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);

    static final MethodHandle READ_MPS = withPath("jaos_read_mps");
    static final MethodHandle READ_LP = withPath("jaos_read_lp");
    static final MethodHandle READ_NL = withPath("jaos_read_nl");
    static final MethodHandle READ_QPLIB = withPath("jaos_read_qplib");
    static final MethodHandle READ_OSIL = withPath("jaos_read_osil");
    static final MethodHandle READ_CBF = withPath("jaos_read_cbf");
    static final MethodHandle WRITE_MPS = withPath("jaos_write_mps");
    static final MethodHandle WRITE_LP = withPath("jaos_write_lp");
    static final MethodHandle WRITE_NL = withPath("jaos_write_nl");
    static final MethodHandle WRITE_QPLIB = withPath("jaos_write_qplib");
    static final MethodHandle WRITE_CBF = withPath("jaos_write_cbf");
    static final MethodHandle WRITE_OSIL = withPath("jaos_write_osil");
    static final MethodHandle WRITE_SOLUTION = withPath("jaos_write_solution");
    static final MethodHandle WRITE_SOL_AMPL = fn("jaos_write_sol_ampl", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle READ_SOLUTION = fn("jaos_read_solution", JAVA_INT, ADDRESS,
        ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle READ_CERTIFICATE = fn("jaos_read_certificate", JAVA_INT, ADDRESS,
        ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle READ_CONE_DUALS = fn("jaos_read_cone_duals", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle READ_BASIS = fn("jaos_read_basis", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle WRITE_MPS_BASIS = withPath("jaos_write_mps_basis");
    static final MethodHandle READ_MPS_BASIS = fn("jaos_read_mps_basis", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle WRITE_POINT = withPath("jaos_write_point");
    static final MethodHandle WRITE_POINT_VALUES = fn("jaos_write_point_values", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle WRITE_DUALS = withPath("jaos_write_duals");
    static final MethodHandle WRITE_DUAL_VALUES = fn("jaos_write_dual_values", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle READ_POINT = fn("jaos_read_point", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle READ_DUALS = fn("jaos_read_duals", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SOLUTION_FILE_STATUS = fn("jaos_solution_file_status", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle WRITE_PROOF = withPath("jaos_write_proof");
    static final MethodHandle CHECK_PROOF = fn("jaos_check_proof", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle READ_OPTIONS = withPath("jaos_read_options");

    static final MethodHandle NUM_COL = fn("jaos_num_col", JAVA_LONG, ADDRESS);
    static final MethodHandle NUM_ROW = fn("jaos_num_row", JAVA_LONG, ADDRESS);
    static final MethodHandle NUM_NZ = fn("jaos_num_nz", JAVA_LONG, ADDRESS);
    static final MethodHandle COL_COST = fn("jaos_col_cost", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle COL_BOUNDS = fn("jaos_col_bounds", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS);
    static final MethodHandle ROW_BOUNDS = fn("jaos_row_bounds", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS);
    static final MethodHandle SET_COL_COST = fn("jaos_set_col_cost", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_DOUBLE);
    static final MethodHandle SET_COL_BOUNDS = fn("jaos_set_col_bounds", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_DOUBLE, JAVA_DOUBLE);
    static final MethodHandle SET_ROW_BOUNDS = fn("jaos_set_row_bounds", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_DOUBLE, JAVA_DOUBLE);
    static final MethodHandle OBJECTIVE_SENSE = fn("jaos_objective_sense", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle OBJECTIVE_OFFSET = fn("jaos_objective_offset", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle SET_OBJECTIVE_SENSE = setter("jaos_set_objective_sense", JAVA_INT);
    static final MethodHandle SET_OBJECTIVE_OFFSET = setter("jaos_set_objective_offset", JAVA_DOUBLE);
    static final MethodHandle COL_ENTRIES = fn("jaos_col_entries", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle ROW_ENTRIES = fn("jaos_row_entries", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle COEFFICIENT = fn("jaos_coefficient", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_LONG, ADDRESS);
    static final MethodHandle SET_COEFFICIENT = fn("jaos_set_coefficient", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_LONG, JAVA_DOUBLE);
    static final MethodHandle ADD_COLS = fn("jaos_add_cols", JAVA_INT, ADDRESS, JAVA_LONG,
        ADDRESS, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle ADD_ROWS = fn("jaos_add_rows", JAVA_INT, ADDRESS, JAVA_LONG,
        ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle DELETE_COLS = fn("jaos_delete_cols", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle DELETE_ROWS = fn("jaos_delete_rows", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);

    static final MethodHandle COL_NAME = fn("jaos_col_name", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG);
    static final MethodHandle ROW_NAME = fn("jaos_row_name", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG);
    static final MethodHandle OBJECTIVE_NAME = fn("jaos_objective_name", JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG);
    static final MethodHandle MODEL_NAME = fn("jaos_model_name", JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG);
    static final MethodHandle SET_COL_NAME = fn("jaos_set_col_name", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle SET_ROW_NAME = fn("jaos_set_row_name", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle SET_OBJECTIVE_NAME = setter("jaos_set_objective_name", ADDRESS);
    static final MethodHandle SET_MODEL_NAME = setter("jaos_set_model_name", ADDRESS);
    static final MethodHandle COL_INDEX = fn("jaos_col_index", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle ROW_INDEX = fn("jaos_row_index", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);

    static final MethodHandle HAS_INTEGER = fn("jaos_model_has_integer", JAVA_BOOLEAN, ADDRESS);
    static final MethodHandle SET_COL_INTEGER = fn("jaos_set_col_integer", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_BOOLEAN);
    static final MethodHandle COL_INTEGER = fn("jaos_col_integer", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle SET_COL_SEMICONTINUOUS = fn("jaos_set_col_semicontinuous", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_BOOLEAN);
    static final MethodHandle COL_SEMICONTINUOUS = fn("jaos_col_semicontinuous", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle ADD_SOS = fn("jaos_add_sos", JAVA_INT, ADDRESS, JAVA_INT, JAVA_LONG, ADDRESS, ADDRESS);
    static final MethodHandle NUM_SOS = fn("jaos_num_sos", JAVA_LONG, ADDRESS);
    static final MethodHandle SOS = fn("jaos_sos", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SET_ROW_INDICATOR = fn("jaos_set_row_indicator", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_LONG, JAVA_INT);
    static final MethodHandle ROW_INDICATOR = fn("jaos_row_indicator", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS);

    static final MethodHandle SET_COL_QUADRATIC = fn("jaos_set_col_quadratic", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_DOUBLE);
    static final MethodHandle COL_QUADRATIC = fn("jaos_col_quadratic", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle SET_QUADRATIC = fn("jaos_set_quadratic", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle QUADRATIC_NZ = fn("jaos_quadratic_nz", JAVA_LONG, ADDRESS);
    static final MethodHandle QUADRATIC = fn("jaos_quadratic", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SET_ROW_QUADRATIC = fn("jaos_set_row_quadratic", JAVA_INT, ADDRESS, JAVA_LONG, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle ROW_QUADRATIC_NZ = fn("jaos_row_quadratic_nz", JAVA_LONG, ADDRESS, JAVA_LONG);
    static final MethodHandle ROW_QUADRATIC = fn("jaos_row_quadratic", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle ADD_CONE = fn("jaos_add_cone", JAVA_INT, ADDRESS, JAVA_INT, JAVA_LONG, ADDRESS);
    static final MethodHandle NUM_CONES = fn("jaos_num_cones", JAVA_LONG, ADDRESS);
    static final MethodHandle CONE = fn("jaos_cone", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle DELETE_CONES = fn("jaos_delete_cones", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle CONE_DUAL = fn("jaos_cone_dual", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);

    static final MethodHandle SET_OPTION = fn("jaos_set_option", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle GET_OPTION = fn("jaos_get_option", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, JAVA_LONG);
    static final MethodHandle SET_TIME_LIMIT = setter("jaos_set_time_limit", JAVA_DOUBLE);
    static final MethodHandle SET_WORK_LIMIT = setter("jaos_set_work_limit", JAVA_LONG);
    static final MethodHandle SET_THREADS = setter("jaos_set_threads", JAVA_LONG);
    static final MethodHandle THREADS_OF = fn("jaos_threads_of", JAVA_LONG, ADDRESS);
    static final MethodHandle SET_PRIMAL_TOLERANCE = setter("jaos_set_primal_tolerance", JAVA_DOUBLE);
    static final MethodHandle SET_DUAL_TOLERANCE = setter("jaos_set_dual_tolerance", JAVA_DOUBLE);
    static final MethodHandle SET_ALGORITHM = setter("jaos_set_algorithm", JAVA_INT);
    static final MethodHandle ALGORITHM_OF = fn("jaos_algorithm_of", JAVA_INT, ADDRESS);

    static final MethodHandle SET_MIP_START = setter("jaos_set_mip_start", ADDRESS);
    static final MethodHandle SET_MIP_CUTOFF = setter("jaos_set_mip_cutoff", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_GAP = setter("jaos_set_mip_gap", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_NODE_LIMIT = setter("jaos_set_mip_node_limit", JAVA_LONG);
    static final MethodHandle SET_MIP_BRANCHING = setter("jaos_set_mip_branching", JAVA_INT);
    static final MethodHandle SET_MIP_RELIABILITY = setter("jaos_set_mip_reliability", JAVA_LONG);
    static final MethodHandle SET_MIP_PROBE_CAP = setter("jaos_set_mip_probe_cap", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_PROBE_DEPTH = setter("jaos_set_mip_probe_depth", JAVA_LONG);
    static final MethodHandle SET_MIP_NODE_SELECT = setter("jaos_set_mip_node_select", JAVA_LONG);
    static final MethodHandle SET_MIP_RESTART = setter("jaos_set_mip_restart", JAVA_INT);
    static final MethodHandle SET_MIP_CONFLICTS = setter("jaos_set_mip_conflicts", JAVA_INT);
    static final MethodHandle SET_MIP_SYMMETRY = setter("jaos_set_mip_symmetry", JAVA_INT);
    static final MethodHandle SET_MIP_ORBITAL = setter("jaos_set_mip_orbital", JAVA_INT);
    static final MethodHandle SET_MIP_PROPAGATE = setter("jaos_set_mip_propagate", JAVA_LONG);
    static final MethodHandle SET_MIP_PROPAGATE_DEPTH = setter("jaos_set_mip_propagate_depth", JAVA_LONG);
    static final MethodHandle SET_MIP_RCFIX = setter("jaos_set_mip_rcfix", JAVA_INT);
    static final MethodHandle SET_MIP_TIGHTEN = setter("jaos_set_mip_tighten", JAVA_INT);
    static final MethodHandle SET_MIP_PROBING = setter("jaos_set_mip_probing", JAVA_INT);
    static final MethodHandle SET_MIP_PROBING_CAP = setter("jaos_set_mip_probing_cap", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_CLIQUE_FIX = setter("jaos_set_mip_clique_fix", JAVA_INT);
    static final MethodHandle SET_MIP_CUT_ROUNDS = setter("jaos_set_mip_cut_rounds", JAVA_LONG);
    static final MethodHandle SET_MIP_CUT_DEPTH = setter("jaos_set_mip_cut_depth", JAVA_LONG);
    static final MethodHandle SET_MIP_CUT_DROP = setter("jaos_set_mip_cut_drop", JAVA_BOOLEAN);
    static final MethodHandle SET_MIP_NODE_CUT_CAP = setter("jaos_set_mip_node_cut_cap", JAVA_LONG);
    static final MethodHandle SET_MIP_COVER_ROUNDS = setter("jaos_set_mip_cover_rounds", JAVA_LONG);
    static final MethodHandle SET_MIP_CLIQUE_ROUNDS = setter("jaos_set_mip_clique_rounds", JAVA_LONG);
    static final MethodHandle SET_MIP_ZERO_HALF_ROUNDS = setter("jaos_set_mip_zero_half_rounds", JAVA_LONG);
    static final MethodHandle SET_MIP_FLOW_COVER_ROUNDS = setter("jaos_set_mip_flow_cover_rounds", JAVA_LONG);
    static final MethodHandle SET_MIP_CUT_STALL = setter("jaos_set_mip_cut_stall", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_NODE_CUT_STALL = setter("jaos_set_mip_node_cut_stall", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_ROOT_CUT_DROP = setter("jaos_set_mip_root_cut_drop", JAVA_INT);
    static final MethodHandle SET_MIP_COVER_LIFT = setter("jaos_set_mip_cover_lift", JAVA_INT);
    static final MethodHandle SET_MIP_MIR_ROUNDS = setter("jaos_set_mip_mir_rounds", JAVA_LONG);
    static final MethodHandle SET_MIP_NODE_MIR = setter("jaos_set_mip_node_mir", JAVA_INT);
    static final MethodHandle SET_MIP_MIR_AGGREGATE = setter("jaos_set_mip_mir_aggregate", JAVA_LONG);
    static final MethodHandle SET_MIP_DIVE = setter("jaos_set_mip_dive", JAVA_BOOLEAN);
    static final MethodHandle SET_MIP_DIVE_BACKTRACK = setter("jaos_set_mip_dive_backtrack", JAVA_LONG);
    static final MethodHandle SET_MIP_DIVE_GAP = setter("jaos_set_mip_dive_gap", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_DIVE_HEURISTIC = setter("jaos_set_mip_dive_heuristic", JAVA_LONG);
    static final MethodHandle SET_MIP_DIVE_HEURISTIC_DEPTH = setter("jaos_set_mip_dive_heuristic_depth", JAVA_LONG);
    static final MethodHandle SET_MIP_DIVE_CHILD = setter("jaos_set_mip_dive_child", JAVA_INT);
    static final MethodHandle SET_MIP_DIVE_DEGRADE = setter("jaos_set_mip_dive_degrade", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_RINS = setter("jaos_set_mip_rins", JAVA_LONG);
    static final MethodHandle SET_MIP_LOCAL_BRANCHING = setter("jaos_set_mip_local_branching", JAVA_LONG);
    static final MethodHandle SET_MIP_FEASPUMP = setter("jaos_set_mip_feaspump", JAVA_LONG);
    static final MethodHandle SET_MIP_PUMP_GENERAL = setter("jaos_set_mip_pump_general", JAVA_INT);
    static final MethodHandle SET_MIP_PUMP_OBJ = setter("jaos_set_mip_pump_obj", JAVA_DOUBLE);
    static final MethodHandle SET_MIP_PUMP_ALWAYS = setter("jaos_set_mip_pump_always", JAVA_INT);
    static final MethodHandle SET_MIP_HEURISTICS = setter("jaos_set_mip_heuristics", JAVA_BOOLEAN);
    static final MethodHandle SET_MIP_TREE_BATCH = setter("jaos_set_mip_tree_batch", JAVA_LONG);
    static final MethodHandle SET_MIP_POOL_SIZE = setter("jaos_set_mip_pool_size", JAVA_LONG);

    static final MethodHandle SET_LOG_CALLBACK = fn("jaos_set_log_callback", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SET_LOG_LEVEL = setter("jaos_set_log_level", JAVA_INT);
    static final MethodHandle SET_PROGRESS_CALLBACK = fn("jaos_set_progress_callback", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SET_INCUMBENT_CALLBACK = fn("jaos_set_incumbent_callback", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SET_NODE_CALLBACK = fn("jaos_set_node_callback", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle NODE_ADD_ROW = fn("jaos_node_add_row", JAVA_INT, ADDRESS, JAVA_LONG,
        ADDRESS, ADDRESS, JAVA_DOUBLE, JAVA_DOUBLE);

    static final MethodHandle SOLVE = fn("jaos_solve", JAVA_INT, ADDRESS);
    static final MethodHandle STATUS_OF = fn("jaos_status_of", JAVA_INT, ADDRESS);
    static final MethodHandle OBJECTIVE = fn("jaos_objective", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle SOLUTION = fn("jaos_solution", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle BASIS = fn("jaos_basis", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle SET_BASIS = fn("jaos_set_basis", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle CLEAR_BASIS = fn("jaos_clear_basis", null, ADDRESS);
    static final MethodHandle ITERATIONS = fn("jaos_iterations", JAVA_LONG, ADDRESS);
    static final MethodHandle WORK_UNITS = fn("jaos_work_units", JAVA_LONG, ADDRESS);
    static final MethodHandle SOLVE_TIME = fn("jaos_solve_time", JAVA_DOUBLE, ADDRESS);
    static final MethodHandle MIP_RESULT = fn("jaos_mip_result", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle MIP_INCUMBENT = fn("jaos_mip_incumbent", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle MIP_POOL_COUNT = fn("jaos_mip_pool_count", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle MIP_POOL_SOLUTION = fn("jaos_mip_pool_solution", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS);
    static final MethodHandle PRESOLVE_RESULT = fn("jaos_presolve_result", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle MODEL_STATISTICS = fn("jaos_model_statistics", JAVA_INT, ADDRESS, ADDRESS);

    static final MethodHandle CERTIFICATE = fn("jaos_certificate", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle UNBOUNDED_RAY = fn("jaos_unbounded_ray", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle CHECK_SOLUTION = fn("jaos_check_solution", JAVA_INT, ADDRESS,
        ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
    static final MethodHandle CHECK_CONIC_SOLUTION = fn("jaos_check_conic_solution", JAVA_INT, ADDRESS,
        ADDRESS, ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
    static final MethodHandle CHECK_CERTIFICATE = fn("jaos_check_certificate", JAVA_INT, ADDRESS,
        ADDRESS, JAVA_DOUBLE, ADDRESS);
    static final MethodHandle CHECK_CONIC_CERTIFICATE = fn("jaos_check_conic_certificate", JAVA_INT, ADDRESS,
        ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
    static final MethodHandle CHECK_RAY = fn("jaos_check_ray", JAVA_INT, ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);

    static final MethodHandle IIS = fn("jaos_iis", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle IIS_MODEL = fn("jaos_iis_model", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle FEASRELAX = fn("jaos_feasrelax", JAVA_INT, ADDRESS, JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle COST_RANGING = fn("jaos_cost_ranging", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle RHS_RANGING = fn("jaos_rhs_ranging", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle BOUND_RANGING = fn("jaos_bound_ranging", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS);

    static final MethodHandle VERIFY = fn("jaos_verify", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle VERIFY_BASIS = fn("jaos_verify_basis", JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS);
    static final MethodHandle EXACT_COL_VALUE = fn("jaos_exact_col_value", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle EXACT_ROW_DUAL = fn("jaos_exact_row_dual", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle EXACT_OBJECTIVE = fn("jaos_exact_objective", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle EXACT_CERTIFICATE = fn("jaos_exact_certificate", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle EXACT_ROW_MULTIPLIER = fn("jaos_exact_row_multiplier", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);
    static final MethodHandle EXACT_UNBOUNDED_RAY = fn("jaos_exact_unbounded_ray", JAVA_INT, ADDRESS, ADDRESS);
    static final MethodHandle EXACT_COL_DIRECTION = fn("jaos_exact_col_direction", JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS);

    static String string(MemorySegment p) {
        return p.equals(MemorySegment.NULL) ? ""
                                            : p.reinterpret(Long.MAX_VALUE).getString(0);
    }
}
