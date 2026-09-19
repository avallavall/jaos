# SPDX-License-Identifier: Apache-2.0

jaos_version <- function() .Call(r_version)

jaos_model <- function() .Call(r_new)

reader_kind <- function(path) {
    p <- sub("\\.gz$", "", path)
    for (k in c("lp", "nl", "qplib", "osil", "cbf"))
        if (endsWith(p, paste0(".", k))) return(k)
    "mps"
}

# Reads a file into a new model, the reader chosen by its name as `jaos
# solve` chooses it: MPS for a name with no known extension.
jaos_read <- function(path) {
    m <- jaos_model()
    .Call(r_read, m, path.expand(path), reader_kind(path))
    m
}

# Writes the model as "mps" or "lp", or the last answer as "solution".
jaos_write <- function(model, path, kind = "mps")
    invisible(.Call(r_write, model, path.expand(path), kind))

# The whole model in one call, the matrix column-wise: 0-based starts, one
# more than the columns, and 0-based row indices.
jaos_load_lp <- function(model, cost, col_lower, col_upper, row_lower,
                         row_upper, a_start, a_index, a_value,
                         maximize = FALSE, offset = 0)
    invisible(.Call(r_load_lp, model, as.integer(maximize), offset, cost,
                    col_lower, col_upper, row_lower, row_upper, a_start,
                    a_index, a_value))

# Marks the columns (1-based) integer.
jaos_set_integer <- function(model, cols)
    invisible(.Call(r_set_integer, model, cols - 1))

# The objective's 1/2 x'Qx as triplets over 1-based columns, one per
# diagonal entry and one per off-diagonal pair.
jaos_set_quadratic <- function(model, rows, cols, values)
    invisible(.Call(r_set_quadratic, model, rows - 1, cols - 1, values))

# Row `row`'s part 1/2 x'Qx, everything 1-based.
jaos_set_row_quadratic <- function(model, row, rows, cols, values)
    invisible(.Call(r_set_row_quadratic, model, row - 1, rows - 1, cols - 1,
                    values))

# A second-order cone over 1-based columns, or a rotated one.
jaos_add_cone <- function(model, cols, rotated = FALSE)
    invisible(.Call(r_add_cone, model, rotated, cols - 1))

jaos_set_option <- function(model, name, value)
    invisible(.Call(r_set_option, model, name,
                    if (is.logical(value)) tolower(value) else format(value)))

jaos_get_option <- function(model, name) .Call(r_get_option, model, name)

jaos_solve <- function(model) invisible(.Call(r_solve, model))

jaos_status <- function(model) .Call(r_status, model)

jaos_objective <- function(model) .Call(r_objective, model)

# c(columns, rows, cones)
jaos_dims <- function(model) .Call(r_dims, model)

# list(x, row_activity, row_dual, reduced_cost) of an optimal solve.
jaos_solution <- function(model) .Call(r_solution, model)

# The dual of cone `k` (1-based), one entry per member.
jaos_cone_dual <- function(model, k) .Call(r_cone_dual, model, k - 1)

# The Farkas multipliers of an infeasible model, or NULL.
jaos_certificate <- function(model) .Call(r_certificate, model)

# The improving direction of an unbounded model, or NULL.
jaos_unbounded_ray <- function(model) .Call(r_unbounded_ray, model)

jaos_mip_result <- function(model) .Call(r_mip_result, model)

# One call from a dense matrix: minimise (or maximise) obj'x + 1/2 x'Qx
# subject to row_lower <= A x <= row_upper and the column bounds, with the
# listed columns (1-based) integer and each element of `cones` a vector of
# 1-based columns in a second-order cone. The answer is a list with the
# status, the objective, the values and the row duals, and the model.
jaos_solve_lp <- function(obj, A, row_lower, row_upper,
                          col_lower = rep(0, length(obj)),
                          col_upper = rep(Inf, length(obj)),
                          integer = NULL, Q = NULL, cones = list(),
                          maximize = FALSE, offset = 0, options = list()) {
    A <- as.matrix(A)
    n <- length(obj)
    stopifnot(ncol(A) == n, nrow(A) == length(row_lower),
              length(row_lower) == length(row_upper))
    start <- numeric(n + 1)
    index <- numeric(0)
    value <- numeric(0)
    for (j in seq_len(n)) {
        nz <- which(A[, j] != 0)
        index <- c(index, nz - 1)
        value <- c(value, A[nz, j])
        start[j + 1] <- length(value)
    }
    m <- jaos_model()
    jaos_load_lp(m, obj, col_lower, col_upper, row_lower, row_upper, start,
                 index, value, maximize = maximize, offset = offset)
    if (length(integer) > 0)
        jaos_set_integer(m, integer)
    if (!is.null(Q)) {
        Q <- as.matrix(Q)
        pairs <- which(lower.tri(Q, diag = TRUE) & Q != 0, arr.ind = TRUE)
        if (nrow(pairs) > 0)
            jaos_set_quadratic(m, pairs[, 1], pairs[, 2], Q[pairs])
    }
    for (cone in cones)
        jaos_add_cone(m, cone)
    for (name in names(options))
        jaos_set_option(m, name, options[[name]])
    jaos_solve(m)
    status <- jaos_status(m)
    out <- list(status = status, model = m)
    if (status == "optimal") {
        s <- jaos_solution(m)
        out$objective <- jaos_objective(m)
        out$x <- s$x
        out$row_dual <- s$row_dual
        out$reduced_cost <- s$reduced_cost
    }
    out
}
