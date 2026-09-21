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

cat(passed, "passed,", failed, "failed\n")
quit(status = if (failed == 0) 0 else 1)
