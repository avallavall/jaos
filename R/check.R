# SPDX-License-Identifier: Apache-2.0
library(jaos)

args <- commandArgs(trailingOnly = TRUE)
data <- if (length(args) > 0) args[1] else "tests/data"
passed <- 0
failed <- 0
check <- function(ok, what) {
    if (isTRUE(ok)) {
        passed <<- passed + 1
        cat("ok  ", what, "\n")
    } else {
        failed <<- failed + 1
        cat("FAIL", what, "\n")
    }
}
near <- function(a, b, tol = 1e-6) isTRUE(all(abs(a - b) <= tol))

check(startsWith(jaos_version(), "0."),
      paste("the library answers with its version", jaos_version()))
check(grepl("^([0-9a-f]{12})?$", jaos_build_commit()),
      paste0("the library names the commit it was built from, '",
             jaos_build_commit(), "'"))

m <- jaos_read(file.path(data, "g1.lp"))
jaos_solve(m)
check(jaos_status(m) == "optimal", "g1.lp solves optimal")
check(near(jaos_objective(m), -5), "to objective -5")
check(near(jaos_solution(m)$x, c(0, -1, 8)), "at (0, -1, 8)")
check(near(jaos_dims(m), c(3, 3, 0)), "with 3 columns, 3 rows and no cone")

err <- tryCatch({ jaos_read(file.path(data, "no_such_file.lp")); "" },
                error = function(e) conditionMessage(e))
check(grepl("cannot open", err), paste("a missing file is an R error:", err))

m <- jaos_model()
err <- tryCatch({ jaos_set_option(m, "no_such_option", 1); "" },
                error = function(e) conditionMessage(e))
check(nchar(err) > 0, "an unknown option is an R error")
jaos_set_option(m, "mip_gap", 0.001)
check(startsWith(jaos_get_option(m, "mip_gap"), "0.001"),
      "an option reads back as it was set")

r <- jaos_solve_lp(obj = c(5, 4, 3, 7, 6), A = matrix(c(2, 3, 1, 4, 5), 1),
                   row_lower = -Inf, row_upper = 9, col_upper = rep(1, 5),
                   integer = 1:5, maximize = TRUE)
check(r$status == "optimal", "a knapsack solves optimal")
check(near(r$objective, 16), "to 16")
check(near(r$x, c(1, 1, 0, 1, 0)), "with the first, second and fourth items")
mr <- jaos_mip_result(r$model)
check(mr$has_incumbent && near(mr$bound, 16, 1e-4), "and the tree's report agrees")

a1 <- c(23, 71, 45, 88, 12, 57, 39, 64, 91, 18, 76, 33)
a2 <- c(54, 17, 82, 29, 66, 41, 95, 13, 58, 87, 24, 70)
b <- c(floor(sum(a1) / 2), floor(sum(a2) / 2))
r <- jaos_solve_lp(obj = c(rep(0, 12), rep(1, 4)),
                   A = rbind(c(a1, 1, -1, 0, 0), c(a2, 0, 0, 1, -1)),
                   row_lower = b, row_upper = b,
                   col_upper = c(rep(1, 12), rep(Inf, 4)), integer = 1:12,
                   options = list(mip_node_limit = 2))
check(r$status == "node limit reached" && is.finite(r$objective) &&
      r$objective >= 0 && length(r$x) == 16,
      "a tree stopped by its node limit still returns its point")

m <- jaos_read(file.path(data, "g1.lp"))
jaos_set_log(m, "summary")
out <- capture.output(jaos_solve(m), type = "message")
check(length(out) > 0, paste("the log prints during a solve,", length(out), "lines"))

r <- jaos_solve_lp(obj = c(-1, -1), A = matrix(c(1, 1), 1), row_lower = -Inf,
                   row_upper = 0.5, Q = diag(2, 2))
check(r$status == "optimal", "a QP solves optimal")
check(near(r$objective, -0.375), "to -0.375")
check(near(r$x, c(0.25, 0.25)), "at (0.25, 0.25)")
check(near(r$row_dual, -0.5), "with the row's dual -0.5")

r <- jaos_solve_lp(obj = c(1, 0, 0), A = rbind(c(0, 1, 0), c(0, 0, 1)),
                   row_lower = c(3, 4), row_upper = c(3, 4),
                   col_lower = rep(-Inf, 3), cones = list(1:3))
check(r$status == "optimal", "a cone model solves optimal")
check(near(r$objective, 5), "to 5")
check(near(jaos_cone_dual(r$model, 1), c(1, -0.6, -0.8)),
      "with the cone's dual (1, -0.6, -0.8)")

r <- jaos_solve_lp(obj = 1, A = matrix(1, 1), row_lower = -Inf,
                   row_upper = 1.4, col_lower = 2.5)
check(r$status == "infeasible", "an infeasible LP says so")
y <- jaos_certificate(r$model)
check(length(y) == 1 && y < 0, "with a Farkas multiplier on its upper side")

fails <- function(expr)
    inherits(tryCatch({ expr; NULL }, error = function(e) e), "error")
quiet <- function(expr) {
    out <- NULL
    capture.output(out <- expr, type = "message")
    out
}
tmp <- function(ext) tempfile(fileext = ext)
solve1 <- function() jaos_read(file.path(data, "solve1.mps"))

check(jaos_infinity() == Inf, "the library's infinity is R's Inf")
check(jaos_status_str(3) == "i/o error", "a status code has its name")
check(all(c("mip_gap", "algorithm", "mip_pool_size") %in% jaos_option_names()),
      paste("the option list names", length(jaos_option_names()), "options"))
m <- jaos_model()
check(fails(jaos_read_options(m, file.path(data, "no_such_file.opt"))) &&
      nchar(jaos_model_error(m)) > 0,
      "a failed call leaves its message on the model")
f <- tmp(".opt")
writeLines(c("# a comment", "mip_gap 0.01"), f)
jaos_read_options(m, f)
check(startsWith(jaos_get_option(m, "mip_gap"), "0.01"),
      "an options file sets what it names")

golden <- function() {
    m <- jaos_model()
    jaos_load_lp(m, c(-1, -2), c(0, 0), c(Inf, Inf), -Inf, 4, c(0, 1, 2),
                 c(0, 0), c(1, 1))
    m
}
m <- golden()
check(jaos_num_nz(m) == 2 && identical(jaos_col_cost(m), c(-1, -2)),
      "a loaded model reads back its nonzeros and costs")
b <- jaos_col_bounds(m)
rb <- jaos_row_bounds(m)
check(identical(b$lower, c(0, 0)) && identical(b$upper, c(Inf, Inf)) &&
      rb$lower == -Inf && rb$upper == 4, "and its column and row bounds")
jaos_solve(m)
check(near(jaos_objective(m), -8), "the golden model solves to -8")
jaos_add_rows(m, -Inf, 3, list(i = 1, j = 2, x = 1))
check(identical(c(jaos_dims(m)[2], jaos_num_nz(m)), c(2, 3)),
      "a row added in triplet form lands")
jaos_solve(m)
check(near(jaos_objective(m), -7), "and binds at -7")
jaos_delete_rows(m, 2)
jaos_solve(m)
check(jaos_dims(m)[2] == 1 && near(jaos_objective(m), -8),
      "deleting it gives -8 back")
jaos_add_cols(m, -3, 0, 2, matrix(1, 1, 1))
jaos_solve(m)
check(jaos_dims(m)[1] == 3 && near(jaos_objective(m), -10),
      "a column added from a dense matrix improves it to -10")
jaos_delete_cols(m, c(2, 3))
jaos_solve(m)
check(jaos_dims(m)[1] == 1 && near(jaos_objective(m), -4),
      "deleting two columns leaves -4")
check(fails(jaos_delete_cols(m, c(1, 1))), "a repeated delete index is an R error")

m <- jaos_model()
jaos_load_lp(m, c(1, 1, 1), rep(0, 3), rep(10, 3), c(0, 0), c(5, 5),
             c(0, 2, 3, 5), c(0, 1, 1, 1, 0), c(1, 0, 3, 4, 2))
e <- jaos_col_entries(m, 3)
check(jaos_num_nz(m) == 4 && identical(e$index, c(1, 2)) &&
      identical(e$value, c(2, 4)),
      "a column reads back ascending, the explicit zero dropped")
e <- jaos_row_entries(m, 1)
check(identical(e$index, c(1, 3)) && identical(e$value, c(1, 2)),
      "a row reads back across")
check(jaos_coefficient(m, 2, 3) == 4 && jaos_coefficient(m, 2, 1) == 0,
      "one coefficient reads back, 0 where there is none")
jaos_set_coefficient(m, 2, 1, 7)
check(identical(jaos_row_entries(m, 2)$index, c(1, 2, 3)),
      "a new coefficient inserts an entry")
check(fails(jaos_col_entries(m, 4)), "a column out of range is an R error")

m <- jaos_model()
jaos_load_lp(m, 1, 0, 3, -Inf, 10, c(0, 1), 0, 1, maximize = TRUE, offset = 10)
check(jaos_sense(m) == "maximize" && jaos_obj_offset(m) == 10,
      "the sense and the constant read back")
jaos_solve(m)
check(near(jaos_objective(m), 13), "max x + 10 over x <= 3 is 13")
jaos_set_sense(m, "minimize")
check(jaos_sense(m) == "minimize" && jaos_status(m) == "not run",
      "a new sense discards the answer")
jaos_solve(m)
check(near(jaos_objective(m), 10), "and the minimum is 10")
jaos_set_obj_offset(m, 0)
jaos_solve(m)
check(near(jaos_objective(m), 0), "and 0 once the constant is gone")
check(fails(jaos_set_obj_offset(m, NaN)), "a NaN constant is an R error")

m <- jaos_model()
jaos_load_lp(m, -1, 0, 3, -Inf, 10, c(0, 1), 0, 1)
jaos_solve(m)
jaos_set_col_bounds(m, 1, 0, 7)
jaos_solve(m)
a <- jaos_objective(m)
jaos_set_col_cost(m, 1, 1)
jaos_solve(m)
check(near(a, -7) && near(jaos_objective(m), 0),
      "a moved bound and a new cost change the answer of the same model")

tb <- function(u) jaos_solve_lp(c(1, 1), rbind(c(1, 2), c(3, 1)),
                                 c(-Inf, -Inf), c(u, 6), maximize = TRUE)
r <- tb(4)
jaos_set_row_bounds(r$model, 1, -Inf, 3)
jaos_solve(r$model)
w <- jaos_result(r$model)
fresh <- tb(3)
check(near(w$objective, fresh$objective, 1e-9) && near(w$x, fresh$x, 1e-9),
      "a row bound moved on jaos_solve_lp's model re-solves warm to a fresh build's answer")

m <- jaos_read(file.path(data, "t1.mps"))
check(identical(jaos_col_name(m), c("X1", "X2", "X3")) &&
      identical(jaos_row_name(m), c("LIM1", "LIM2", "EQ1")) &&
      jaos_objective_name(m) == "COST", "a file's names come through")
check(jaos_col_index(m, "X2") == 2 && jaos_row_index(m, "EQ1") == 3,
      "and look up to 1-based indices")
check(fails(jaos_row_index(m, "COST")), "the objective's name is no row's")
check(jaos_model_name(m) == "T1", "the model's name is the file's")
jaos_set_model_name(m, "renamed")
k <- jaos_copy(m)
check(jaos_model_name(k) == "renamed" && jaos_col_name(k, 3) == "X3" &&
      jaos_status(k) == "not run", "a copy keeps the names and not the answer")
jaos_set_col_name(k, 1, "mine")
check(jaos_col_name(m, 1) == "X1", "and is a model of its own")
m <- golden()
check(identical(jaos_col_name(m), c("C1", "C2")), "unnamed columns are positional")
jaos_set_col_name(m, 2, "y")
jaos_set_row_name(m, 1, "cap")
jaos_set_objective_name(m, "profit")
check(jaos_col_name(m, 2) == "y" && jaos_row_name(m, 1) == "cap" &&
      jaos_objective_name(m) == "profit" && jaos_col_index(m, "y") == 2,
      "names set here read back")
jaos_set_col_name(m, 2, NA)
check(jaos_col_name(m, 2) == "C2", "NA gives a column its positional name back")
check(fails(jaos_set_col_name(m, 1, "a b")), "a name with a space is an R error")

m <- jaos_read(file.path(data, "solve1.mps"))
jaos_solve(m)
for (kind in c("mps", "lp")) {
    f <- tmp(paste0(".", kind))
    jaos_write(m, f, kind)
    b <- jaos_read(f)
    jaos_solve(b)
    check(identical(jaos_col_name(b), jaos_col_name(m)) &&
          near(jaos_objective(b), jaos_objective(m), 1e-9),
          paste("an", toupper(kind), "file written here reads back and solves the same"))
}

m <- jaos_read(file.path(data, "t_lin.nl"))
f <- tmp(".nl")
jaos_write(m, f, "nl")
b <- jaos_read(f)
jaos_solve(b)
check(identical(jaos_dims(b), c(3, 3, 0)) && jaos_num_nz(b) == 6 &&
      jaos_col_name(b, 2) == "z" && jaos_integer(b, 3) &&
      near(jaos_objective(b), -4),
      "an NL file written here reads back with its names and integers last")
m <- jaos_read(file.path(data, "g_quad.qplib"))
check(jaos_col_quadratic(m, 1) == 2, "a QPLIB file's quadratic diagonal reads back")
jaos_solve(m)
f <- tmp(".qplib")
jaos_write(m, f, "qplib")
b <- jaos_read(f)
check(jaos_row_name(b, 1) == "c1" && jaos_col_quadratic(b, 2) == 2,
      "a QPLIB file written here reads back")
f <- tmp(".osil")
jaos_write(m, f, "osil")
b <- jaos_read(f)
jaos_solve(b)
check(jaos_col_name(b, 1) == "x" && near(jaos_objective(b), 4, 1e-6),
      "an OSiL file written here reads back and solves to 4")
m <- jaos_read(file.path(data, "g_cone.mps"))
f <- tmp(".cbf")
jaos_write(m, f, "cbf")
b <- jaos_read(f)
jaos_solve(b)
check(identical(jaos_cone(b, 1), list(type = "quadratic", cols = c(1, 2, 3))) &&
      near(jaos_objective(b), 5, 1e-7),
      "a CBF file written here reads back with its cone")
check(fails(jaos_write(m, f, "xyz")), "an unknown kind is an R error")
m <- jaos_read(file.path(data, "t_ampl_lp.nl"))
jaos_solve(m)
f <- tmp(".sol")
jaos_write_sol_ampl(m, f)
lines <- readLines(f)
check(lines[1] == sprintf("JAOS %s: optimal; objective -7", jaos_version()) &&
      lines[17] == "objno 0 0",
      "the AMPL .sol file carries the message and the result code")
jaos_write_sol_ampl(m, f, "solved by hand")
check(readLines(f)[1] == "solved by hand", "and a message of the caller's")

m <- solve1()
jaos_solve(m)
s <- jaos_solution(m)
b <- jaos_basis(m)
check(length(b$col_status) == 3 &&
      all(c(b$col_status, b$row_status) %in%
          c("basic", "at_lower", "at_upper", "free")),
      "the basis comes back one status per variable")
f <- tmp(".sol")
jaos_write(m, f, "solution")
rs <- jaos_read_solution(m, f)
check(rs$objective == jaos_objective(m) && identical(rs$x, s$x) &&
      identical(rs$row_dual, s$row_dual) &&
      identical(rs$reduced_cost, s$reduced_cost) &&
      identical(rs$row_activity, s$row_activity) &&
      identical(rs$col_status, b$col_status) &&
      identical(rs$row_status, b$row_status),
      "a solution file reads back as the same answer")
check(identical(jaos_read_basis(m, f), b) &&
      jaos_solution_file_status(m, f) == "optimal",
      "and carries its basis and its status")
fb <- tmp(".bas")
jaos_write_mps_basis(m, fb)
got <- jaos_read_mps_basis(m, fb)
check(identical(got, b), "an MPS basis file round trips")
n <- solve1()
jaos_set_basis(n, got$col_status, got$row_status)
jaos_solve(n)
check(jaos_status(n) == "optimal" && jaos_iterations(n) == 0,
      "and warm-starts a new model with no iteration")
jaos_clear_basis(n)
jaos_solve(n)
check(near(jaos_objective(n), 29) && jaos_iterations(n) > 0,
      "a cleared basis solves cold")
check(fails(jaos_set_basis(n, "basic", character(0))),
      "a basis of the wrong length is an R error")
fp <- tmp(".txt")
fd <- tmp(".txt")
jaos_write_point(m, fp)
jaos_write_duals(m, fd)
check(identical(jaos_read_point(m, fp), s$x) &&
      identical(jaos_read_duals(m, fd), s$row_dual),
      "the point and duals files round trip")
jaos_write_point(m, fp, c(1.5, -2, 0))
jaos_write_duals(m, fd, rep(0, jaos_dims(m)[2]))
check(identical(jaos_read_point(m, fp), c(1.5, -2, 0)) &&
      all(jaos_read_duals(m, fd) == 0),
      "and are written from values of the caller's")
check(fails(jaos_write_point(m, fp, 1)), "a point of the wrong length is an R error")

m <- jaos_read(file.path(data, "t1.mps"))
jaos_solve(m)
want <- jaos_certificate(m)
f <- tmp(".sol")
jaos_write(m, f, "solution")
rc <- jaos_read_certificate(m, f)
check(jaos_solution_file_status(m, f) == "infeasible" &&
      rc$status == "infeasible" && identical(rc$ray, want),
      "an infeasible answer's file carries its certificate")
check(jaos_check_certificate(m, rc$ray)$certified,
      "which the checker certifies")

norm <- function() {
    m <- jaos_model()
    jaos_load_lp(m, c(1, 0, 0), rep(-Inf, 3), rep(Inf, 3), c(3, 4), c(3, 4),
                 c(0, 0, 1, 2), c(0, 1), c(1, 1))
    jaos_add_cone(m, 1:3)
    m
}
m <- norm()
check(jaos_statistics(m)$cone_set == 1 &&
      identical(jaos_cone(m, 1), list(type = "quadratic", cols = c(1, 2, 3))),
      "a cone reads back its type and members")
jaos_solve(m)
s <- jaos_solution(m)
z <- jaos_cone_dual(m, 1)
ck <- jaos_check_conic_solution(m, s$x, s$row_dual, list(z))
check(ck$primal_feasible && ck$dual_feasible && ck$checked_duals &&
      ck$max_cone_violation <= 1e-7,
      "the conic checker accepts the cone's answer")
check(fails(jaos_check_conic_solution(m, s$x, s$row_dual, list(z[1:2]))),
      "a cone vector of the wrong length is an R error")
f <- tmp(".sol")
jaos_write(m, f, "solution")
check(identical(jaos_read_cone_duals(m, f), list(z)),
      "the cone duals read back from the solution file")
check(fails(jaos_delete_cols(m, 3)), "a column in a cone cannot be deleted")
jaos_delete_cones(m, 1)
check(jaos_dims(m)[3] == 0 && fails(jaos_cone(m, 1)),
      "deleting the cone leaves none")
m <- jaos_model()
jaos_load_lp(m, c(0, 0), c(-Inf, 2), c(1, 2), numeric(0), numeric(0),
             numeric(0), numeric(0), numeric(0))
jaos_add_cone(m, 1:2)
jaos_solve(m)
z <- jaos_cone_dual(m, 1)
check(jaos_status(m) == "infeasible" &&
      jaos_check_conic_certificate(m, numeric(0), list(z))$certified &&
      !jaos_check_conic_certificate(m, numeric(0), list(c(-1, 0)))$certified,
      "an infeasible cone's certificate is certified, and a wrong one is not")

m <- solve1()
jaos_solve(m)
s <- jaos_solution(m)
ck <- jaos_check_solution(m, s$x, s$row_dual)
check(length(ck) == 20 && ck$primal_feasible && ck$dual_feasible &&
      ck$checked_duals && near(ck$primal_objective, 29, 1e-9),
      "the checker accepts the true answer, in a report of 20 fields")
ck <- jaos_check_solution(m, s$x + 100, s$row_dual)
check(!ck$primal_feasible && max(ck$max_col_violation, ck$max_row_violation) > 1,
      "and flags a corrupted one")
check(!jaos_check_solution(m, s$x)$checked_duals, "no duals means no dual verdict")
m <- jaos_model()
jaos_load_lp(m, 1, 0, 2, 4, Inf, c(0, 1), 0, 1)
jaos_solve(m)
ck <- jaos_check_certificate(m, jaos_certificate(m))
check(ck$certified && near(ck$inf_rows, 4, 1e-9) &&
      near(ck$sup_columns, 2, 1e-9) && near(ck$gap, 2, 1e-9),
      "an infeasible model's Farkas ray is certified")
m <- jaos_model()
jaos_load_lp(m, -1, 0, Inf, -Inf, Inf, c(0, 1), 0, 1)
jaos_solve(m)
d <- jaos_unbounded_ray(m)
rr <- jaos_check_ray(m, d)
check(jaos_status(m) == "unbounded" && d > 0 && rr$certified && rr$rate < 0 &&
      rr$max_col_escape == 0, "an unbounded model's ray is certified")

m <- solve1()
check(fails(jaos_verify(m)), "the proof needs an optimum")
jaos_solve(m)
v <- jaos_verify(m)
check(v$capacity_bits == 4096 && v$status == "optimal" && v$stage == "none" &&
      is.na(v$at_row) && is.na(v$at_col) && v$violation == 0,
      "solve1's optimum is proved, with each field where jaos.h puts it")
third <- function() {
    m <- jaos_model()
    jaos_load_lp(m, 1, 0, Inf, 1, Inf, c(0, 1), 0, 3, offset = 0.5)
    jaos_set_col_name(m, 1, "x")
    m
}
m <- third()
check(fails(jaos_exact_col_value(m)), "no exact value before a proof")
jaos_solve(m)
check(jaos_verify(m)$status == "optimal" && jaos_exact_col_value(m) == "1/3" &&
      jaos_exact_row_dual(m) == "1/3" && jaos_exact_objective(m) == "5/6",
      "a third comes back as a third, and the objective as 5/6")
f <- tmp(".proof")
jaos_write_proof(m, f)
body <- readLines(f)
check(any(grepl("proof optimal", body)) && any(grepl("1/3", body)),
      "the proof file holds the exact values")
q <- third()
pr <- jaos_check_proof(q, f)
check(pr$primal && pr$dual && pr$objective && pr$certified &&
      is.na(pr$bad_row) && pr$terms > 0 && pr$kind == "optimal",
      "a fresh model checks the proof file")
writeLines(sub("col x 1/3", "col x 1/4", body, fixed = TRUE), f)
pr <- jaos_check_proof(q, f)
check(!pr$primal && !pr$certified && pr$kind == "optimal",
      "and refuses a tampered one")
m <- jaos_model()
jaos_load_lp(m, c(-1, -1), c(0, 0), c(3, Inf), -Inf, 4, c(0, 1, 2), c(0, 0),
             c(1, 1))
v <- jaos_verify_basis(m, c("at_upper", "basic"), "at_upper")
check(v$status == "optimal" && jaos_status(m) == "not run" &&
      jaos_exact_col_value(m, 1) == "3" && jaos_exact_objective(m) == "-4",
      "a basis from outside is proved with no solve")
v <- jaos_verify_basis(m, c("at_lower", "at_lower"), "basic")
check(v$status == "broken" && v$stage == "dual",
      "and the slack basis is broken on the dual side")
m <- jaos_build_lp(c(1, 1), rbind(c(1, 1), c(1, 1)), c(-Inf, 2), c(1, Inf))
jaos_solve(m)
e <- jaos_exact_certificate(m)
check(e$derived && e$bound_bits <= e$capacity_bits &&
      identical(jaos_exact_row_multiplier(m), c("-1", "1")),
      "the Farkas multipliers are derived exactly")
jaos_write_proof(m, f)
pr <- jaos_check_proof(m, f)
check(pr$kind == "infeasible" && pr$certified, "and their proof file is certified")
m <- jaos_build_lp(c(-1, 0), matrix(c(1, -1), 1), -Inf, 1)
jaos_solve(m)
e <- jaos_exact_unbounded_ray(m)
dx <- jaos_exact_col_direction(m)
check(jaos_status(m) == "unbounded" && e$derived && is.na(e$at_row) &&
      dx[1] == dx[2], "the unbounded direction is derived exactly")
jaos_write_proof(m, f)
pr <- jaos_check_proof(m, f)
check(pr$kind == "unbounded" && pr$certified, "and its proof file is certified")

m <- jaos_model()
jaos_load_lp(m, 1, 0, Inf, c(1, -Inf), c(Inf, 0), c(0, 2), c(0, 1), c(1, 1))
jaos_solve(m)
found <- jaos_iis(m)
check(identical(found$row_side, c("lower", "upper")) &&
      identical(found$col_side, "none") && found$report$members == 2 &&
      found$report$solves == 3 && found$report$from_certificate,
      "the IIS is the two rows and not the column bound")
check(jaos_status(m) == "infeasible" && length(jaos_certificate(m)) == 2,
      "and leaves the model's answer as it was")
sub <- jaos_iis_model(m, found)
check(jaos_solve(sub) == "infeasible" && identical(jaos_dims(sub)[1:2], c(1, 2)),
      "the subsystem comes back as a model, and it is infeasible")
found$row_side[1] <- "none"
check(jaos_solve(jaos_iis_model(m, found)) == "optimal",
      "and one member fewer is feasible")
m <- solve1()
jaos_solve(m)
check(fails(jaos_iis(m)), "a feasible model has no IIS")
big <- function(lo) jaos_build_lp(c(1, 1), matrix(c(1, 1), 1), lo, Inf,
                                  col_upper = c(10, 10))
r <- jaos_feasrelax(big(30))
check(near(r$report$total, 10, 1e-9) && near(r$row_move, -10, 1e-9) &&
      identical(r$col_move, c(0, 0)) && r$report$status == "optimal",
      "the feasibility relaxation moves the row by 10")
check(jaos_solve(big(30 + r$row_move)) == "optimal",
      "and the moved row is feasible")
rows <- jaos_feasrelax(big(30), "rows")
cols <- jaos_feasrelax(big(30), "cols")
check(rows$report$cols_moved == 0 && rows$report$rows_moved == 1 &&
      cols$report$rows_moved == 0 && cols$report$cols_moved == 1 &&
      near(rows$report$total, cols$report$total, 1e-9),
      "the scope decides which bound moves")

m <- jaos_build_lp(c(1, 1), rbind(c(1, 2), c(3, 1)), c(-Inf, -Inf), c(4, 6),
                   maximize = TRUE)
check(fails(jaos_cost_ranging(m)), "ranging needs an optimum")
jaos_solve(m)
cr <- jaos_cost_ranging(m)
check(near(cr$lower, c(0.5, 1 / 3), 1e-12) && near(cr$upper, c(3, 2), 1e-12),
      "cost ranging reads the textbook intervals")
rr <- jaos_rhs_ranging(m)
check(near(rr$upper_lo[1], 2, 1e-12) && near(rr$upper_hi[1], 12, 1e-12) &&
      rr$lower_lo[2] == -Inf && near(rr$lower_hi[2], 6, 1e-12),
      "and rhs ranging")
br <- jaos_bound_ranging(m)
check(br$lower_lo[1] == -Inf && near(br$lower_hi[1], 1.6, 1e-12) &&
      near(br$upper_lo[2], 1.2, 1e-12) && br$upper_hi[2] == Inf,
      "and bound ranging")

kn3 <- function(...) jaos_solve_lp(c(10, 13, 7), matrix(c(3, 5, 2), 1), -Inf, 8,
                                   col_upper = rep(1, 3), integer = 1:3,
                                   maximize = TRUE, ...)
st <- jaos_statistics(jaos_build_lp(c(10, 13, 7), matrix(c(3, 5, 2), 1), -Inf,
                                    8, col_upper = rep(1, 3), integer = 1:3))
check(length(st) == 26 && st$num_col == 3 && st$num_row == 1 &&
      st$integer_col == 3 && st$binary_col == 3 && st$one_sided_row == 1 &&
      st$num_row == st$equality_row + st$ranged_row + st$one_sided_row +
                     st$free_row, "the statistics count the knapsack")
r <- jaos_solve_lp(c(1, 1), rbind(c(1, 1), c(1, 0)), c(3, -Inf), c(Inf, 10))
pr <- jaos_presolve_report(r$model)
check(length(pr) == 18 && pr$rounds >= 0 && pr$num_row <= 2 &&
      pr$duplicate_row == 0, "the presolve report reaches R")
best <- kn3()
r <- kn3(start = c(1, 1, 0))
check(r$status == "optimal" && near(best$objective, 23) &&
      near(r$objective, best$objective) &&
      jaos_mip_result(r$model)$start_accepted,
      "a MIP start keeps the optimum, and the report says it was taken")
r <- kn3(start = c(1, 1, NaN))
check(r$status == "optimal" && near(r$objective, best$objective) &&
      jaos_mip_result(r$model)$start_accepted,
      "a partial start is completed and taken")
r <- kn3(start = c(9, 9, 9))
check(r$status == "optimal" && near(r$objective, best$objective) &&
      !jaos_mip_result(r$model)$start_accepted,
      "and a start that is no integer point is passed over")
check(fails(jaos_set_mip_start(r$model, 1)),
      "a start of the wrong length is an R error")
mr <- jaos_mip_result(r$model)
check(length(mr) == 13 && mr$lp_solves >= 1 &&
      all(c("cuts", "heuristic_points", "fixed_cols", "tightened",
            "symmetry_generators", "symmetry_orbits",
            "start_accepted") %in% names(mr)),
      "the MIP report carries all 13 fields")
off <- list(mip_cut_rounds = 0, mip_cover_rounds = 0, mip_mir_rounds = 0,
            mip_cut_depth = 0)
r <- jaos_solve_lp(c(10, 13, 7, 9, 5), matrix(c(3, 5, 2, 4, 2), 1), -Inf, 8,
                   col_upper = rep(1, 5), integer = 1:5, maximize = TRUE,
                   options = c(off, mip_pool_size = 3))
objs <- vapply(r$pool, function(p) p$objective, 0)
check(r$status == "optimal" && length(r$pool) >= 1 && length(r$pool) <= 3 &&
      near(objs[1], 23) && identical(r$pool[[1]]$x[1:2], c(1, 1)) &&
      !is.unsorted(rev(objs)), "the solution pool keeps the best points, best first")
check(identical(jaos_mip_pool(r$model), r$pool), "and reads back from the model")

sides <- c(-Inf, -Inf)
dense <- jaos_solve_lp(c(1, 1), rbind(c(1, 2), c(3, 1)), sides, c(4, 6),
                       maximize = TRUE)
trip <- jaos_solve_lp(c(1, 1), list(i = c(1, 2, 1, 2), j = c(1, 1, 2, 2),
                                    x = c(1, 3, 2, 1)), sides, c(4, 6),
                      maximize = TRUE)
colf <- jaos_solve_lp(c(1, 1), list(p = c(0, 2, 4), i = c(0, 1, 0, 1),
                                    x = c(1, 3, 2, 1)), sides, c(4, 6),
                      maximize = TRUE)
slam <- jaos_solve_lp(c(1, 1), list(i = c(2, 1, 1, 2), j = c(1, 1, 2, 2),
                                    v = c(3, 1, 2, 1), nrow = 2, ncol = 2),
                      sides, c(4, 6), maximize = TRUE)
check(near(dense$x, c(1.6, 1.2), 1e-9) && identical(trip$x, dense$x) &&
      identical(colf$x, dense$x) && identical(slam$x, dense$x),
      "a triplet, a column form and an unsorted slam triplet give the dense answer")
check(fails(jaos_solve_lp(c(1, 1), list(i = 3, j = 1, x = 1), sides, c(4, 6))),
      "an entry outside the matrix is an R error")
check(near(dense$row_activity, c(4, 6), 1e-9) && dense$work_units > 0 &&
      dense$iterations >= 0 && dense$solve_time >= 0 &&
      jaos_work_units(dense$model) == dense$work_units,
      "the result carries the row activity and the counters")
check(!jaos_has_integer(dense$model), "an LP has no integer structure")

for (t in 1:2) {
    r <- jaos_solve_lp(c(1, 1, 1), matrix(1, 1, 3), -Inf, 10,
                       col_upper = rep(1, 3), maximize = TRUE,
                       sos = list(list(type = t, cols = 1:3)))
    nz <- which(abs(r$x) > 1e-9)
    check(r$status == "optimal" && near(r$objective, t) && length(nz) == t &&
          (t == 1 || diff(nz) == 1) && jaos_num_sos(r$model) == 1 &&
          identical(jaos_sos(r$model, 1),
                    list(type = as.numeric(t), cols = c(1, 2, 3),
                         weights = c(1, 2, 3))) && jaos_has_integer(r$model),
          paste0("an SOS", t, " set limits the nonzero members and reads back"))
}
ind <- function(v) jaos_solve_lp(c(1, -3), matrix(c(1, 0), 1), -Inf, 2,
                                 col_upper = c(10, 1), integer = 2,
                                 maximize = TRUE,
                                 indicators = list(list(row = 1, col = 2,
                                                        value = v)))
r <- ind(1)
check(near(r$objective, 10) && identical(r$x, c(10, 0)) &&
      identical(jaos_row_indicator(r$model, 1), list(col = 2, value = 1)),
      "an indicator row holds only while its column says so")
jaos_set_row_indicator(r$model, 1, NA)
check(is.na(jaos_row_indicator(r$model, 1)$col), "and NA makes it a plain row")
r <- ind(0)
check(near(r$objective, 7) && identical(r$x, c(10, 1)),
      "an indicator on the value 0 holds the other way")
r <- jaos_solve_lp(c(1, 5), matrix(c(1, 1), 1), 1, Inf, col_lower = c(2, 0),
                   col_upper = c(10, 1), semicontinuous = 1)
check(near(r$objective, 2) && identical(r$x, c(2, 0)) &&
      identical(jaos_semicontinuous(r$model), c(TRUE, FALSE)) &&
      identical(jaos_integer(r$model), c(FALSE, FALSE)),
      "a semi-continuous column rests at zero or above its floor")
jaos_set_col_cost(r$model, 1, 10)
jaos_solve(r$model)
w <- jaos_result(r$model)
check(near(w$objective, 5) && identical(w$x, c(0, 1)),
      "and at zero once its floor costs more, on the same model")
jaos_set_semicontinuous(r$model, 1, FALSE)
check(!jaos_semicontinuous(r$model, 1), "the mark comes off")

m <- jaos_model()
jaos_load_lp(m, c(-3, -3), c(0, 0), c(3, 3), -Inf, 4, c(0, 1, 2), c(0, 0),
             c(1, 1))
jaos_set_quadratic(m, c(1, 2, 2), c(1, 2, 1), c(2, 2, 1))
q <- jaos_quadratic(m)
check(jaos_quadratic_nz(m) == 3 && all(q$rows >= q$cols) &&
      any(q$rows == 2 & q$cols == 1 & q$values == 1),
      "the objective's Q reads back as its lower triangle")
jaos_solve(m)
check(near(jaos_objective(m), -3, 1e-5), "and solves to -3")
jaos_set_col_quadratic(m, 1, 4)
check(jaos_col_quadratic(m, 1) == 4, "a column's quadratic term reads back")
r <- jaos_solve_lp(c(-1, -1), matrix(0, 1, 2), -Inf, 2,
                   col_lower = c(-Inf, -Inf),
                   quadratic_rows = list(list(row = 1, Q = diag(2, 2))))
check(r$status == "optimal" && near(r$objective, -2, 1e-7) &&
      near(r$x, c(1, 1), 1e-7) && near(r$row_activity, 2, 1e-7) &&
      near(r$row_dual, -0.5, 1e-6), "a quadratic row reaches jaos_solve_lp")
check(jaos_row_quadratic_nz(r$model, 1) == 2 &&
      identical(jaos_row_quadratic(r$model, 1),
                list(rows = c(1, 2), cols = c(1, 2), values = c(2, 2))) &&
      jaos_statistics(r$model)$quadratic_row == 1 &&
      jaos_check_conic_solution(r$model, r$x, r$row_dual, list())$dual_feasible,
      "and reads back, and checks")
r <- jaos_solve_lp(c(1, 0, 0), matrix(0, 0, 3), numeric(0), numeric(0),
                   col_lower = c(0, 1, 3), col_upper = c(Inf, 1, 3),
                   rotated_cones = list(1:3))
check(r$status == "optimal" && near(r$objective, 4.5, 1e-7) &&
      jaos_cone(r$model, 1)$type == "rotated",
      "a rotated cone reaches jaos_solve_lp and halves the square")

m <- solve1()
seen <- list()
jaos_set_progress_callback(m, function(p) {
    seen[[length(seen) + 1]] <<- p
    NULL
})
check(jaos_solve(m) == "optimal" && length(seen) > 0 &&
      identical(names(seen[[1]]),
                c("iterations", "work_units", "primal_infeasibility",
                  "nodes", "bound", "has_incumbent", "incumbent")) &&
      all(vapply(seen, function(p) p$nodes == 0 && !p$has_incumbent, TRUE)) &&
      all(vapply(seen, function(p) p$iterations >= 0, TRUE)),
      paste("the progress callback sees the solve,", length(seen), "calls"))
u <- solve1()
jaos_solve(u)
check(jaos_work_units(m) == jaos_work_units(u) &&
      jaos_objective(m) == jaos_objective(u), "watching does not change the answer")
m <- solve1()
jaos_set_progress_callback(m, function(p) "stop")
check(jaos_solve(m) == "interrupted by callback" && fails(jaos_objective(m)),
      "returning \"stop\" interrupts the solve")
jaos_set_progress_callback(m, NULL)
check(jaos_solve(m) == "optimal", "and removing the callback lets it finish")
m <- solve1()
jaos_set_progress_callback(m, function(p) stop("broken on purpose"))
check(quiet(jaos_solve(m)) == "interrupted by callback",
      "an R error in a callback stops the solve and does not unwind through it")
m <- solve1()
calls <- 0
jaos_set_progress_callback(m, function(p) {
    calls <<- calls + 1
    NULL
})
k <- jaos_copy(m)
rm(m)
invisible(gc())
check(jaos_solve(k) == "optimal" && calls > 0,
      "a copy carries the callback and keeps its function alive")
batched <- function(threads) {
    half <- c(floor(sum(a1) / 2), floor(sum(a2) / 2))
    m <- jaos_build_lp(c(rep(0, 12), rep(1, 4)),
                       rbind(c(a1, 1, -1, 0, 0), c(a2, 0, 0, 1, -1)), half, half,
                       col_upper = c(rep(1, 12), rep(Inf, 4)), integer = 1:12,
                       options = list(mip_tree_batch = 4, threads = threads))
    calls <- 0
    jaos_set_progress_callback(m, function(p) {
        calls <<- calls + 1
        NULL
    })
    jaos_solve(m)
    list(status = jaos_status(m), work = jaos_work_units(m), calls = calls)
}
one <- batched(1)
four <- batched(4)
check(four$status == "optimal" && four$work == one$work &&
      four$calls > 0 && four$calls < one$calls,
      paste("a tree solved on four threads calls back on R's thread only,",
            four$calls, "of", one$calls, "calls"))

m <- jaos_build_lp(c(1, 1), rbind(c(1, 1), c(1, 0), c(0, 1)), rep(-Inf, 3),
                   c(3.6, 2.2, 1.4), integer = 1:2, maximize = TRUE,
                   options = c(off, mip_node_limit = 1))
seen <- list()
jaos_set_incumbent_callback(m, function(inc) {
    seen[[length(seen) + 1]] <<- inc
    NULL
})
check(jaos_solve(m) == "node limit reached" && length(seen) == 1 &&
      seen[[1]]$node == 1 && seen[[1]]$by_rounding &&
      identical(seen[[1]]$x, c(2, 1)) &&
      near(jaos_mip_incumbent(m)$objective, 3),
      "the incumbent callback sees the tree's point")
jaos_set_option(m, "mip_node_limit", 0)
jaos_set_incumbent_callback(m, function(inc) "stop")
check(jaos_solve(m) == "interrupted by callback" &&
      jaos_mip_result(m)$has_incumbent,
      "and \"stop\" ends the tree with its incumbent")

m <- jaos_build_lp(c(2, 2, 1), matrix(1, 1, 3), -Inf, 2, col_upper = rep(1, 3),
                   integer = 1:3, maximize = TRUE)
seen <- logical(0)
kept <- NULL
jaos_set_node_callback(m, function(ev) {
    seen <<- c(seen, ev$integral)
    kept <<- ev
    if (ev$integral && ev$x[1] + ev$x[2] > 1.5)
        jaos_node_add_row(ev, c(1, 2), c(1, 1), upper = 1)
    NULL
})
jaos_solve(m)
r <- jaos_result(m)
check(r$status == "optimal" && near(r$objective, 3) &&
      r$x[1] + r$x[2] <= 1 + 1e-9 && any(seen),
      "the node callback adds a lazy row")
check(fails(jaos_node_add_row(kept, 1, 1)),
      "an event kept past its callback refuses a row")
jaos_set_node_callback(m, NULL)
jaos_solve(m)
check(near(jaos_objective(m), 4), "and without the callback the row is gone")
h <- jaos_build_lp(c(10, 13, 7, 8, 11, 5), matrix(c(4, 6, 3, 4, 5, 2), 1, 6),
                   -Inf, 12, col_upper = rep(1, 6), integer = 1:6,
                   maximize = TRUE,
                   options = list(mip_cut_rounds = 0, mip_cover_rounds = 0,
                                  mip_mir_rounds = 0, mip_heuristics = FALSE,
                                  mip_node_limit = 1))
short_refused <- FALSE
jaos_set_node_callback(h, function(ev) {
    jaos_node_add_solution(ev, c(1, 0, 1, 0, 1, 0))
    short_refused <<- fails(jaos_node_add_solution(ev, c(1, 0, 1)))
    NULL
})
jaos_solve(h)
hr <- jaos_mip_result(h)
check(jaos_status(h) == "node limit reached" && hr$has_incumbent &&
      near(hr$incumbent, 28) && short_refused,
      "the node callback hands the tree a solution it keeps as the incumbent")
alloff <- c(off, list(mip_clique_rounds = 0, mip_zero_half_rounds = 0,
                      mip_flow_cover_rounds = 0, mip_heuristics = FALSE,
                      mip_dive_heuristic = 0, mip_feaspump = 0,
                      mip_tighten = 0))
q <- jaos_build_lp(c(3, 2.5, 2), matrix(2, 1, 3), -Inf, 3, col_upper = rep(1, 3),
                   integer = 1:3, maximize = TRUE, options = alloff)
jaos_solve(q)
plain <- jaos_mip_result(q)$nodes
choices <- numeric(0)
jaos_set_node_callback(q, function(ev) {
    if (ev$node == 1 && !ev$integral) {
        choices <<- c(choices, ev$branch_col)
        jaos_node_add_row(ev, 1:3, c(1, 1, 1), upper = 1)
    }
    NULL
})
jaos_solve(q)
check(plain > 1 && near(jaos_objective(q), 3) &&
      jaos_mip_result(q)$nodes == 1 && choices[1] == 2,
      "a user cut closes the root, and the event names the branch column")
s <- jaos_build_lp(c(3, 1, 1), rbind(c(2, 2, 0), c(2, 0, 2)), c(-Inf, -Inf),
                   c(3, 3), col_upper = rep(1, 3), integer = 1:3,
                   maximize = TRUE, options = alloff)
steered <- numeric(0)
depth1 <- numeric(0)
jaos_set_node_callback(s, function(ev) {
    if (ev$depth == 0 && !ev$integral) {
        other <- if (identical(ev$branch_col, 2)) 3 else 2
        steered <<- c(steered, other)
        ev$branch_col <- other
    }
    if (ev$depth == 1)
        depth1 <<- c(depth1, ev$x[steered[1]])
    NULL
})
jaos_solve(s)
check(near(jaos_objective(s), 3) && length(depth1) > 0 &&
      all(depth1 %in% c(0, 1)), "the node callback steers the branching")
jaos_set_node_callback(q, function(ev) jaos_node_add_row(ev, 8, 1, 0, 1))
check(quiet(jaos_solve(q)) == "interrupted by callback",
      "a row the tree refuses stops the solve")
jaos_set_node_callback(q, function(ev) "stop")
check(jaos_solve(q) == "interrupted by callback", "and \"stop\" ends it")

m <- jaos_read(file.path(data, "g1.lp"))
got <- character(0)
jaos_set_log(m, "summary", function(level, line) got <<- c(got, level))
jaos_solve(m)
check(length(got) > 0 && all(got %in% c("summary", "progress", "detail")),
      "the log reaches a function of the caller's, with each line's level")
jaos_set_log(m, "off")
got <- character(0)
jaos_solve(m)
check(length(got) == 0, "and stops when turned off")

cat(passed, "passed,", failed, "failed\n")
quit(status = if (failed == 0) 0 else 1)
