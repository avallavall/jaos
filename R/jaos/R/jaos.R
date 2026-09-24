# SPDX-License-Identifier: Apache-2.0

basis_names <- c("basic", "at_lower", "at_upper", "free")

side_names <- c("none", "lower", "upper", "both")

writer_kinds <- c("mps", "lp", "nl", "qplib", "cbf", "osil", "solution")

codes <- function(s, labels)
    if (is.character(s)) match(s, labels) - 1L else as.integer(s)

widen <- function(v, n) if (length(v) == 1) rep(v, n) else v

cols_or_all <- function(model, cols)
    if (is.null(cols)) seq_len(jaos_dims(model)[1]) else cols

rows_or_all <- function(model, rows)
    if (is.null(rows)) seq_len(jaos_dims(model)[2]) else rows

as_triplet <- function(A, nr, nc) {
    if (is.null(A))
        return(list(i = numeric(0), j = numeric(0), x = numeric(0)))
    if (isS4(A) && inherits(A, "dgCMatrix"))
        A <- list(p = A@p, i = A@i, x = A@x, dim = A@Dim)
    else if (isS4(A) && inherits(A, "dgTMatrix"))
        A <- list(i = A@i + 1, j = A@j + 1, x = A@x, dim = A@Dim)
    if (is.list(A) && !is.data.frame(A)) {
        d <- if (!is.null(A$dim)) A$dim else c(A$nrow, A$ncol)
        if (length(d) == 2 && (d[1] != nr || d[2] != nc))
            stop("the matrix is ", d[1], " by ", d[2], ", and the model needs ",
                 nr, " by ", nc)
        if (!is.null(A$p)) {
            if (length(A$p) != nc + 1)
                stop("the column starts need ", nc + 1, " values")
            t <- list(i = A$i + 1, j = rep(seq_len(nc), diff(A$p)), x = A$x)
        } else {
            t <- list(i = A$i, j = A$j, x = if (is.null(A$x)) A$v else A$x)
        }
    } else {
        A <- as.matrix(A)
        if (nrow(A) != nr || ncol(A) != nc)
            stop("the matrix is ", nrow(A), " by ", ncol(A),
                 ", and the model needs ", nr, " by ", nc)
        nz <- which(A != 0, arr.ind = TRUE)
        t <- list(i = nz[, 1], j = nz[, 2], x = A[nz])
    }
    if (length(t$i) != length(t$x) || length(t$j) != length(t$x))
        stop("the matrix needs one row index and one column index per value")
    if (any(t$i < 1 | t$i > nr) || any(t$j < 1 | t$j > nc))
        stop("the matrix has an entry outside its ", nr, " rows and ", nc,
             " columns")
    t
}

csc <- function(t, nmajor) {
    o <- order(t$j, t$i)
    list(start = c(0, cumsum(tabulate(t$j, nbins = nmajor))),
         index = t$i[o] - 1, value = as.numeric(t$x[o]))
}

lower_part <- function(Q, n) {
    t <- as_triplet(Q, n, n)
    keep <- t$i >= t$j & t$x != 0
    list(i = t$i[keep], j = t$j[keep], x = t$x[keep])
}

cone_vector <- function(model, parts) {
    parts <- as.list(parts)
    k <- jaos_dims(model)[3]
    if (length(parts) != k)
        stop("one vector per cone: the model has ", k, " cones, and ",
             length(parts), " vectors came")
    for (i in seq_len(k)) {
        want <- length(jaos_cone(model, i)$cols)
        if (length(parts[[i]]) != want)
            stop("cone ", i, " has ", want, " members, and its vector ",
                 length(parts[[i]]), " values")
    }
    as.numeric(unlist(parts))
}

jaos_version <- function() .Call(r_version)

jaos_build_commit <- function() .Call(r_build_commit)

jaos_infinity <- function() .Call(r_infinity)

jaos_status_str <- function(code) .Call(r_status_str, as.integer(code))

jaos_model_error <- function(model) .Call(r_model_error, model)

jaos_option_names <- function() .Call(r_option_names)

jaos_model <- function() .Call(r_new)

jaos_copy <- function(model) .Call(r_copy, model)

reader_kind <- function(path) {
    p <- sub("\\.gz$", "", path)
    for (k in c("lp", "nl", "qplib", "osil", "cbf"))
        if (endsWith(p, paste0(".", k))) return(k)
    "mps"
}

jaos_read <- function(path) {
    m <- jaos_model()
    .Call(r_read, m, path.expand(path), reader_kind(path))
    m
}

jaos_write <- function(model, path, kind = "mps") {
    if (!(kind %in% writer_kinds))
        stop("kind is one of ", paste(writer_kinds, collapse = ", "))
    invisible(.Call(r_write, model, path.expand(path), kind))
}

jaos_load_lp <- function(model, cost, col_lower, col_upper, row_lower,
                         row_upper, a_start, a_index, a_value,
                         maximize = FALSE, offset = 0)
    invisible(.Call(r_load_lp, model, as.integer(maximize), offset, cost,
                    col_lower, col_upper, row_lower, row_upper, a_start,
                    a_index, a_value))

jaos_dims <- function(model) .Call(r_dims, model)

jaos_num_nz <- function(model) .Call(r_num_nz, model)

jaos_col_cost <- function(model, cols = NULL)
    .Call(r_col_cost, model, cols_or_all(model, cols) - 1)

jaos_set_col_cost <- function(model, cols, cost)
    invisible(.Call(r_set_col_cost, model, cols - 1,
                    widen(cost, length(cols))))

jaos_col_bounds <- function(model, cols = NULL)
    .Call(r_bounds, model, cols_or_all(model, cols) - 1, FALSE)

jaos_set_col_bounds <- function(model, cols, lower, upper)
    invisible(.Call(r_set_bounds, model, cols - 1, widen(lower, length(cols)),
                    widen(upper, length(cols)), FALSE))

jaos_row_bounds <- function(model, rows = NULL)
    .Call(r_bounds, model, rows_or_all(model, rows) - 1, TRUE)

jaos_set_row_bounds <- function(model, rows, lower, upper)
    invisible(.Call(r_set_bounds, model, rows - 1, widen(lower, length(rows)),
                    widen(upper, length(rows)), TRUE))

jaos_sense <- function(model) .Call(r_sense, model)

jaos_set_sense <- function(model, sense) {
    if (!(sense %in% c("minimize", "maximize")))
        stop("sense is \"minimize\" or \"maximize\"")
    invisible(.Call(r_set_sense, model, sense == "maximize"))
}

jaos_obj_offset <- function(model) .Call(r_offset, model)

jaos_set_obj_offset <- function(model, offset)
    invisible(.Call(r_set_offset, model, as.numeric(offset)))

jaos_col_entries <- function(model, col)
    .Call(r_entries, model, col - 1, FALSE)

jaos_row_entries <- function(model, row)
    .Call(r_entries, model, row - 1, TRUE)

jaos_coefficient <- function(model, row, col)
    .Call(r_coefficient, model, row - 1, col - 1)

jaos_set_coefficient <- function(model, row, col, value)
    invisible(.Call(r_set_coefficient, model, row - 1, col - 1,
                    as.numeric(value)))

jaos_add_cols <- function(model, cost, lower, upper, A = NULL) {
    n <- length(cost)
    a <- csc(as_triplet(A, jaos_dims(model)[2], n), n)
    invisible(.Call(r_add_cols, model, cost, widen(lower, n),
                    widen(upper, n), a$start, a$index, a$value))
}

jaos_add_rows <- function(model, lower, upper, A = NULL) {
    n <- length(lower)
    t <- as_triplet(A, n, jaos_dims(model)[1])
    a <- csc(list(i = t$j, j = t$i, x = t$x), n)
    invisible(.Call(r_add_rows, model, lower, widen(upper, n), a$start,
                    a$index, a$value))
}

jaos_delete_cols <- function(model, cols)
    invisible(.Call(r_delete, model, cols - 1, FALSE))

jaos_delete_rows <- function(model, rows)
    invisible(.Call(r_delete, model, rows - 1, TRUE))

jaos_col_name <- function(model, cols = NULL)
    .Call(r_names, model, cols_or_all(model, cols) - 1, FALSE)

jaos_row_name <- function(model, rows = NULL)
    .Call(r_names, model, rows_or_all(model, rows) - 1, TRUE)

jaos_set_col_name <- function(model, cols, names)
    invisible(.Call(r_set_names, model, cols - 1, as.character(names), FALSE))

jaos_set_row_name <- function(model, rows, names)
    invisible(.Call(r_set_names, model, rows - 1, as.character(names), TRUE))

jaos_col_index <- function(model, names)
    .Call(r_index, model, as.character(names), FALSE)

jaos_row_index <- function(model, names)
    .Call(r_index, model, as.character(names), TRUE)

jaos_objective_name <- function(model) .Call(r_name, model, 0L)

jaos_set_objective_name <- function(model, name)
    invisible(.Call(r_set_name, model, 0L, as.character(name)))

jaos_model_name <- function(model) .Call(r_name, model, 1L)

jaos_set_model_name <- function(model, name)
    invisible(.Call(r_set_name, model, 1L, as.character(name)))

jaos_set_integer <- function(model, cols, on = TRUE)
    invisible(.Call(r_set_flags, model, cols - 1,
                    widen(as.logical(on), length(cols)), FALSE))

jaos_integer <- function(model, cols = NULL)
    .Call(r_flags, model, cols_or_all(model, cols) - 1, FALSE)

jaos_set_semicontinuous <- function(model, cols, on = TRUE)
    invisible(.Call(r_set_flags, model, cols - 1,
                    widen(as.logical(on), length(cols)), TRUE))

jaos_semicontinuous <- function(model, cols = NULL)
    .Call(r_flags, model, cols_or_all(model, cols) - 1, TRUE)

jaos_has_integer <- function(model) .Call(r_has_integer, model)

jaos_add_sos <- function(model, type, cols, weights = seq_along(cols))
    invisible(.Call(r_add_sos, model, as.integer(type), cols - 1, weights))

jaos_num_sos <- function(model) .Call(r_num_sos, model)

jaos_sos <- function(model, k) .Call(r_sos, model, k - 1)

jaos_set_row_indicator <- function(model, row, col, value = 1)
    invisible(.Call(r_set_row_indicator, model, row - 1,
                    if (is.null(col) || is.na(col)) -1 else col - 1,
                    as.integer(value)))

jaos_row_indicator <- function(model, row)
    .Call(r_row_indicator, model, row - 1)

jaos_set_quadratic <- function(model, rows, cols, values)
    invisible(.Call(r_set_quadratic, model, rows - 1, cols - 1, values))

jaos_quadratic <- function(model) .Call(r_quadratic, model, NULL)

jaos_quadratic_nz <- function(model) .Call(r_quadratic_nz, model, NULL)

jaos_set_col_quadratic <- function(model, cols, q)
    invisible(.Call(r_set_col_quadratic, model, cols - 1,
                    widen(q, length(cols))))

jaos_col_quadratic <- function(model, cols = NULL)
    .Call(r_col_quadratic, model, cols_or_all(model, cols) - 1)

jaos_set_row_quadratic <- function(model, row, rows, cols, values)
    invisible(.Call(r_set_row_quadratic, model, row - 1, rows - 1, cols - 1,
                    values))

jaos_row_quadratic <- function(model, row)
    .Call(r_quadratic, model, row - 1)

jaos_row_quadratic_nz <- function(model, row)
    .Call(r_quadratic_nz, model, row - 1)

jaos_add_cone <- function(model, cols, rotated = FALSE)
    invisible(.Call(r_add_cone, model, rotated, cols - 1))

jaos_cone <- function(model, k) .Call(r_cone, model, k - 1)

jaos_delete_cones <- function(model, k)
    invisible(.Call(r_delete_cones, model, k - 1))

jaos_set_option <- function(model, name, value)
    invisible(.Call(r_set_option, model, name,
                    if (is.logical(value)) tolower(value) else format(value)))

jaos_get_option <- function(model, name) .Call(r_get_option, model, name)

jaos_read_options <- function(model, path)
    invisible(.Call(r_read_options, model, path.expand(path)))

jaos_set_log <- function(model, level = "summary", fn = NULL) {
    levels <- c(off = 0L, summary = 1L, progress = 2L, detail = 3L)
    stopifnot(level %in% names(levels))
    invisible(.Call(r_set_log, model, levels[[level]], fn))
}

jaos_set_progress_callback <- function(model, fn)
    invisible(.Call(r_set_callback, model, 1L, fn))

jaos_set_incumbent_callback <- function(model, fn)
    invisible(.Call(r_set_callback, model, 2L, fn))

jaos_set_node_callback <- function(model, fn)
    invisible(.Call(r_set_callback, model, 3L, fn))

jaos_node_add_row <- function(event, cols, values, lower = -Inf, upper = Inf)
    invisible(.Call(r_node_add_row,
                    get0(".event", envir = event, inherits = FALSE),
                    cols - 1, values, as.numeric(lower), as.numeric(upper)))

jaos_node_add_solution <- function(event, values)
    invisible(.Call(r_node_add_solution,
                    get0(".event", envir = event, inherits = FALSE),
                    as.numeric(values)))

jaos_solve <- function(model) {
    .Call(r_solve, model)
    invisible(jaos_status(model))
}

jaos_status <- function(model) .Call(r_status, model)

jaos_objective <- function(model) .Call(r_objective, model)

jaos_solution <- function(model) .Call(r_solution, model)

jaos_iterations <- function(model) .Call(r_counter, model, 0L)

jaos_work_units <- function(model) .Call(r_counter, model, 1L)

jaos_solve_time <- function(model) .Call(r_counter, model, 2L)

jaos_basis <- function(model) .Call(r_basis, model)

jaos_set_basis <- function(model, col_status, row_status)
    invisible(.Call(r_set_basis, model, codes(col_status, basis_names),
                    codes(row_status, basis_names)))

jaos_clear_basis <- function(model) invisible(.Call(r_clear_basis, model))

jaos_cone_dual <- function(model, k) .Call(r_cone_dual, model, k - 1)

jaos_certificate <- function(model) .Call(r_certificate, model)

jaos_unbounded_ray <- function(model) .Call(r_unbounded_ray, model)

jaos_mip_result <- function(model) .Call(r_mip_result, model)

jaos_mip_incumbent <- function(model) .Call(r_mip_incumbent, model)

jaos_mip_pool <- function(model) .Call(r_mip_pool, model)

jaos_set_mip_start <- function(model, x)
    invisible(.Call(r_set_mip_start, model, x))

jaos_write_sol_ampl <- function(model, path, message = NULL)
    invisible(.Call(r_write_sol_ampl, model, path.expand(path),
                    if (is.null(message)) NULL else as.character(message)))

jaos_read_solution <- function(model, path)
    .Call(r_read_solution, model, path.expand(path))

jaos_read_certificate <- function(model, path)
    .Call(r_read_certificate, model, path.expand(path))

jaos_read_cone_duals <- function(model, path)
    .Call(r_read_cone_duals, model, path.expand(path))

jaos_read_basis <- function(model, path)
    .Call(r_read_basis, model, path.expand(path), FALSE)

jaos_write_mps_basis <- function(model, path)
    invisible(.Call(r_write, model, path.expand(path), "mps_basis"))

jaos_read_mps_basis <- function(model, path)
    .Call(r_read_basis, model, path.expand(path), TRUE)

jaos_write_point <- function(model, path, x = NULL)
    invisible(if (is.null(x)) .Call(r_write, model, path.expand(path), "point")
              else .Call(r_write_values, model, path.expand(path), x, FALSE))

jaos_read_point <- function(model, path)
    .Call(r_read_vector, model, path.expand(path), FALSE)

jaos_write_duals <- function(model, path, row_dual = NULL)
    invisible(if (is.null(row_dual))
                  .Call(r_write, model, path.expand(path), "duals")
              else .Call(r_write_values, model, path.expand(path), row_dual,
                         TRUE))

jaos_read_duals <- function(model, path)
    .Call(r_read_vector, model, path.expand(path), TRUE)

jaos_solution_file_status <- function(model, path)
    .Call(r_solution_file_status, model, path.expand(path))

jaos_check_solution <- function(model, x, row_dual = NULL, tol = 1e-7)
    .Call(r_check_solution, model, x, row_dual, NULL, tol)

jaos_check_conic_solution <- function(model, x, row_dual, cone_dual,
                                      tol = 1e-7)
    .Call(r_check_solution, model, x, row_dual, cone_vector(model, cone_dual),
          tol)

jaos_check_certificate <- function(model, row_ray, tol = 1e-7)
    .Call(r_check_certificate, model, row_ray, NULL, tol)

jaos_check_conic_certificate <- function(model, row_ray, cone_ray,
                                         tol = 1e-7)
    .Call(r_check_certificate, model, row_ray, cone_vector(model, cone_ray),
          tol)

jaos_check_ray <- function(model, col_ray, tol = 1e-7)
    .Call(r_check_ray, model, col_ray, tol)

jaos_verify <- function(model) .Call(r_verify, model, NULL, NULL)

jaos_verify_basis <- function(model, col_status, row_status)
    .Call(r_verify, model, codes(col_status, basis_names),
          codes(row_status, basis_names))

jaos_exact_col_value <- function(model, cols = NULL)
    .Call(r_exact, model, cols_or_all(model, cols) - 1, 0L)

jaos_exact_row_dual <- function(model, rows = NULL)
    .Call(r_exact, model, rows_or_all(model, rows) - 1, 1L)

jaos_exact_objective <- function(model) .Call(r_exact, model, 0, 2L)

jaos_exact_certificate <- function(model) .Call(r_exact_ray, model, FALSE)

jaos_exact_row_multiplier <- function(model, rows = NULL)
    .Call(r_exact, model, rows_or_all(model, rows) - 1, 3L)

jaos_exact_unbounded_ray <- function(model) .Call(r_exact_ray, model, TRUE)

jaos_exact_col_direction <- function(model, cols = NULL)
    .Call(r_exact, model, cols_or_all(model, cols) - 1, 4L)

jaos_write_proof <- function(model, path)
    invisible(.Call(r_write, model, path.expand(path), "proof"))

jaos_check_proof <- function(model, path)
    .Call(r_check_proof, model, path.expand(path))

jaos_iis <- function(model) .Call(r_iis, model)

jaos_iis_model <- function(model, iis)
    .Call(r_iis_model, model, codes(iis$row_side, side_names),
          codes(iis$col_side, side_names))

jaos_feasrelax <- function(model, scope = "both") {
    scopes <- c(rows = 1L, cols = 2L, both = 3L)
    if (!(scope %in% names(scopes)))
        stop("scope is \"rows\", \"cols\" or \"both\"")
    .Call(r_feasrelax, model, scopes[[scope]])
}

jaos_cost_ranging <- function(model) .Call(r_cost_ranging, model)

jaos_rhs_ranging <- function(model) .Call(r_bound_ranging, model, TRUE)

jaos_bound_ranging <- function(model) .Call(r_bound_ranging, model, FALSE)

jaos_statistics <- function(model) .Call(r_statistics, model)

jaos_presolve_report <- function(model) .Call(r_presolve_report, model)

jaos_result <- function(model) {
    status <- jaos_status(model)
    out <- list(status = status, model = model)
    if (status == "optimal") {
        s <- jaos_solution(model)
        out$objective <- jaos_objective(model)
        out$x <- s$x
        out$row_activity <- s$row_activity
        out$row_dual <- s$row_dual
        out$reduced_cost <- s$reduced_cost
    } else {
        inc <- jaos_mip_incumbent(model)
        if (!is.null(inc)) {
            out$objective <- inc$objective
            out$x <- inc$x
        }
    }
    if (jaos_has_integer(model))
        out$pool <- jaos_mip_pool(model)
    out$iterations <- jaos_iterations(model)
    out$work_units <- jaos_work_units(model)
    out$solve_time <- jaos_solve_time(model)
    out
}

jaos_build_lp <- function(obj, A, row_lower, row_upper,
                          col_lower = rep(0, length(obj)),
                          col_upper = rep(Inf, length(obj)),
                          integer = NULL, Q = NULL, cones = list(),
                          maximize = FALSE, offset = 0, options = list(),
                          rotated_cones = list(), quadratic_rows = list(),
                          sos = list(), indicators = list(),
                          semicontinuous = NULL, start = NULL) {
    n <- length(obj)
    nr <- length(row_lower)
    a <- csc(as_triplet(A, nr, n), n)
    m <- jaos_model()
    jaos_load_lp(m, obj, widen(col_lower, n), widen(col_upper, n), row_lower,
                 widen(row_upper, nr), a$start, a$index, a$value,
                 maximize = maximize, offset = offset)
    if (length(integer) > 0)
        jaos_set_integer(m, integer)
    if (length(semicontinuous) > 0)
        jaos_set_semicontinuous(m, semicontinuous)
    if (!is.null(Q)) {
        q <- lower_part(Q, n)
        if (length(q$x) > 0)
            jaos_set_quadratic(m, q$i, q$j, q$x)
    }
    for (e in quadratic_rows) {
        q <- lower_part(e$Q, n)
        jaos_set_row_quadratic(m, e$row, q$i, q$j, q$x)
    }
    for (cone in cones)
        jaos_add_cone(m, cone)
    for (cone in rotated_cones)
        jaos_add_cone(m, cone, rotated = TRUE)
    for (s in sos)
        jaos_add_sos(m, s$type, s$cols,
                     if (is.null(s$weights)) seq_along(s$cols) else s$weights)
    for (e in indicators)
        jaos_set_row_indicator(m, e$row, e$col,
                               if (is.null(e$value)) 1 else e$value)
    for (name in names(options))
        jaos_set_option(m, name, options[[name]])
    if (!is.null(start))
        jaos_set_mip_start(m, start)
    m
}

jaos_solve_lp <- function(obj, A, row_lower, row_upper,
                          col_lower = rep(0, length(obj)),
                          col_upper = rep(Inf, length(obj)),
                          integer = NULL, Q = NULL, cones = list(),
                          maximize = FALSE, offset = 0, options = list(),
                          rotated_cones = list(), quadratic_rows = list(),
                          sos = list(), indicators = list(),
                          semicontinuous = NULL, start = NULL) {
    m <- jaos_build_lp(obj, A, row_lower, row_upper, col_lower, col_upper,
                       integer, Q, cones, maximize, offset, options,
                       rotated_cones, quadratic_rows, sos, indicators,
                       semicontinuous, start)
    jaos_solve(m)
    jaos_result(m)
}
