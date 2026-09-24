\name{jaos}
\alias{jaos}
\alias{jaos_version}
\alias{jaos_infinity}
\alias{jaos_status_str}
\alias{jaos_model_error}
\alias{jaos_option_names}
\alias{jaos_model}
\alias{jaos_copy}
\alias{jaos_read}
\alias{jaos_write}
\alias{jaos_load_lp}
\alias{jaos_dims}
\alias{jaos_num_nz}
\alias{jaos_col_cost}
\alias{jaos_set_col_cost}
\alias{jaos_col_bounds}
\alias{jaos_set_col_bounds}
\alias{jaos_row_bounds}
\alias{jaos_set_row_bounds}
\alias{jaos_sense}
\alias{jaos_set_sense}
\alias{jaos_obj_offset}
\alias{jaos_set_obj_offset}
\alias{jaos_col_entries}
\alias{jaos_row_entries}
\alias{jaos_coefficient}
\alias{jaos_set_coefficient}
\alias{jaos_add_cols}
\alias{jaos_add_rows}
\alias{jaos_delete_cols}
\alias{jaos_delete_rows}
\alias{jaos_col_name}
\alias{jaos_row_name}
\alias{jaos_set_col_name}
\alias{jaos_set_row_name}
\alias{jaos_col_index}
\alias{jaos_row_index}
\alias{jaos_objective_name}
\alias{jaos_set_objective_name}
\alias{jaos_model_name}
\alias{jaos_set_model_name}
\alias{jaos_set_integer}
\alias{jaos_integer}
\alias{jaos_set_semicontinuous}
\alias{jaos_semicontinuous}
\alias{jaos_has_integer}
\alias{jaos_add_sos}
\alias{jaos_num_sos}
\alias{jaos_sos}
\alias{jaos_set_row_indicator}
\alias{jaos_row_indicator}
\alias{jaos_set_quadratic}
\alias{jaos_quadratic}
\alias{jaos_quadratic_nz}
\alias{jaos_set_col_quadratic}
\alias{jaos_col_quadratic}
\alias{jaos_set_row_quadratic}
\alias{jaos_row_quadratic}
\alias{jaos_row_quadratic_nz}
\alias{jaos_add_cone}
\alias{jaos_cone}
\alias{jaos_delete_cones}
\alias{jaos_set_option}
\alias{jaos_get_option}
\alias{jaos_read_options}
\alias{jaos_set_log}
\alias{jaos_set_progress_callback}
\alias{jaos_set_incumbent_callback}
\alias{jaos_set_node_callback}
\alias{jaos_node_add_row}
\alias{jaos_node_add_solution}
\alias{jaos_solve}
\alias{jaos_status}
\alias{jaos_objective}
\alias{jaos_solution}
\alias{jaos_iterations}
\alias{jaos_work_units}
\alias{jaos_solve_time}
\alias{jaos_basis}
\alias{jaos_set_basis}
\alias{jaos_clear_basis}
\alias{jaos_cone_dual}
\alias{jaos_certificate}
\alias{jaos_unbounded_ray}
\alias{jaos_mip_result}
\alias{jaos_mip_incumbent}
\alias{jaos_mip_pool}
\alias{jaos_set_mip_start}
\alias{jaos_write_sol_ampl}
\alias{jaos_read_solution}
\alias{jaos_read_certificate}
\alias{jaos_read_cone_duals}
\alias{jaos_read_basis}
\alias{jaos_write_mps_basis}
\alias{jaos_read_mps_basis}
\alias{jaos_write_point}
\alias{jaos_read_point}
\alias{jaos_write_duals}
\alias{jaos_read_duals}
\alias{jaos_solution_file_status}
\alias{jaos_check_solution}
\alias{jaos_check_conic_solution}
\alias{jaos_check_certificate}
\alias{jaos_check_conic_certificate}
\alias{jaos_check_ray}
\alias{jaos_verify}
\alias{jaos_verify_basis}
\alias{jaos_exact_col_value}
\alias{jaos_exact_row_dual}
\alias{jaos_exact_objective}
\alias{jaos_exact_certificate}
\alias{jaos_exact_row_multiplier}
\alias{jaos_exact_unbounded_ray}
\alias{jaos_exact_col_direction}
\alias{jaos_write_proof}
\alias{jaos_check_proof}
\alias{jaos_iis}
\alias{jaos_iis_model}
\alias{jaos_feasrelax}
\alias{jaos_cost_ranging}
\alias{jaos_rhs_ranging}
\alias{jaos_bound_ranging}
\alias{jaos_statistics}
\alias{jaos_presolve_report}
\alias{jaos_result}
\alias{jaos_build_lp}
\alias{jaos_solve_lp}
\title{The JAOS Solver: Linear, Mixed-Integer, Quadratic and Conic Programs}
\description{
The JAOS C library from R. A model is read from an MPS, LP, NL, QPLIB, OSiL
or CBF file, or built from vectors, then solved, and its values, duals,
certificates and branch-and-bound report are read back. Every answer is
bit-identical on every machine and is judged by an independent checker.
Every call of the C API has an R function here.
}
\usage{
jaos_version()
jaos_infinity()
jaos_status_str(code)
jaos_model_error(model)
jaos_option_names()

jaos_model()
jaos_copy(model)
jaos_read(path)
jaos_write(model, path, kind = "mps")
jaos_load_lp(model, cost, col_lower, col_upper, row_lower, row_upper,
             a_start, a_index, a_value, maximize = FALSE, offset = 0)
jaos_dims(model)
jaos_num_nz(model)
jaos_col_cost(model, cols = NULL)
jaos_set_col_cost(model, cols, cost)
jaos_col_bounds(model, cols = NULL)
jaos_set_col_bounds(model, cols, lower, upper)
jaos_row_bounds(model, rows = NULL)
jaos_set_row_bounds(model, rows, lower, upper)
jaos_sense(model)
jaos_set_sense(model, sense)
jaos_obj_offset(model)
jaos_set_obj_offset(model, offset)
jaos_col_entries(model, col)
jaos_row_entries(model, row)
jaos_coefficient(model, row, col)
jaos_set_coefficient(model, row, col, value)
jaos_add_cols(model, cost, lower, upper, A = NULL)
jaos_add_rows(model, lower, upper, A = NULL)
jaos_delete_cols(model, cols)
jaos_delete_rows(model, rows)

jaos_col_name(model, cols = NULL)
jaos_row_name(model, rows = NULL)
jaos_set_col_name(model, cols, names)
jaos_set_row_name(model, rows, names)
jaos_col_index(model, names)
jaos_row_index(model, names)
jaos_objective_name(model)
jaos_set_objective_name(model, name)
jaos_model_name(model)
jaos_set_model_name(model, name)

jaos_set_integer(model, cols, on = TRUE)
jaos_integer(model, cols = NULL)
jaos_set_semicontinuous(model, cols, on = TRUE)
jaos_semicontinuous(model, cols = NULL)
jaos_has_integer(model)
jaos_add_sos(model, type, cols, weights = seq_along(cols))
jaos_num_sos(model)
jaos_sos(model, k)
jaos_set_row_indicator(model, row, col, value = 1)
jaos_row_indicator(model, row)

jaos_set_quadratic(model, rows, cols, values)
jaos_quadratic(model)
jaos_quadratic_nz(model)
jaos_set_col_quadratic(model, cols, q)
jaos_col_quadratic(model, cols = NULL)
jaos_set_row_quadratic(model, row, rows, cols, values)
jaos_row_quadratic(model, row)
jaos_row_quadratic_nz(model, row)
jaos_add_cone(model, cols, rotated = FALSE)
jaos_cone(model, k)
jaos_delete_cones(model, k)

jaos_set_option(model, name, value)
jaos_get_option(model, name)
jaos_read_options(model, path)
jaos_set_log(model, level = "summary", fn = NULL)
jaos_set_progress_callback(model, fn)
jaos_set_incumbent_callback(model, fn)
jaos_set_node_callback(model, fn)
jaos_node_add_row(event, cols, values, lower = -Inf, upper = Inf)
jaos_node_add_solution(event, values)

jaos_solve(model)
jaos_status(model)
jaos_objective(model)
jaos_solution(model)
jaos_iterations(model)
jaos_work_units(model)
jaos_solve_time(model)
jaos_basis(model)
jaos_set_basis(model, col_status, row_status)
jaos_clear_basis(model)
jaos_cone_dual(model, k)
jaos_certificate(model)
jaos_unbounded_ray(model)
jaos_mip_result(model)
jaos_mip_incumbent(model)
jaos_mip_pool(model)
jaos_set_mip_start(model, x)

jaos_write_sol_ampl(model, path, message = NULL)
jaos_read_solution(model, path)
jaos_read_certificate(model, path)
jaos_read_cone_duals(model, path)
jaos_read_basis(model, path)
jaos_write_mps_basis(model, path)
jaos_read_mps_basis(model, path)
jaos_write_point(model, path, x = NULL)
jaos_read_point(model, path)
jaos_write_duals(model, path, row_dual = NULL)
jaos_read_duals(model, path)
jaos_solution_file_status(model, path)

jaos_check_solution(model, x, row_dual = NULL, tol = 1e-7)
jaos_check_conic_solution(model, x, row_dual, cone_dual, tol = 1e-7)
jaos_check_certificate(model, row_ray, tol = 1e-7)
jaos_check_conic_certificate(model, row_ray, cone_ray, tol = 1e-7)
jaos_check_ray(model, col_ray, tol = 1e-7)

jaos_verify(model)
jaos_verify_basis(model, col_status, row_status)
jaos_exact_col_value(model, cols = NULL)
jaos_exact_row_dual(model, rows = NULL)
jaos_exact_objective(model)
jaos_exact_certificate(model)
jaos_exact_row_multiplier(model, rows = NULL)
jaos_exact_unbounded_ray(model)
jaos_exact_col_direction(model, cols = NULL)
jaos_write_proof(model, path)
jaos_check_proof(model, path)

jaos_iis(model)
jaos_iis_model(model, iis)
jaos_feasrelax(model, scope = "both")
jaos_cost_ranging(model)
jaos_rhs_ranging(model)
jaos_bound_ranging(model)
jaos_statistics(model)
jaos_presolve_report(model)

jaos_result(model)
jaos_build_lp(obj, A, row_lower, row_upper,
              col_lower = rep(0, length(obj)),
              col_upper = rep(Inf, length(obj)),
              integer = NULL, Q = NULL, cones = list(),
              maximize = FALSE, offset = 0, options = list(),
              rotated_cones = list(), quadratic_rows = list(),
              sos = list(), indicators = list(),
              semicontinuous = NULL, start = NULL)
jaos_solve_lp(obj, A, row_lower, row_upper,
              col_lower = rep(0, length(obj)),
              col_upper = rep(Inf, length(obj)),
              integer = NULL, Q = NULL, cones = list(),
              maximize = FALSE, offset = 0, options = list(),
              rotated_cones = list(), quadratic_rows = list(),
              sos = list(), indicators = list(),
              semicontinuous = NULL, start = NULL)
}
\arguments{
\item{code}{a status code of the C library, 0 to 4.}
\item{model}{a model, from \code{jaos_model}, \code{jaos_read},
  \code{jaos_copy}, \code{jaos_iis_model} or \code{jaos_build_lp}.}
\item{path}{a file name. \code{jaos_read} picks the reader by its
  extension, as \code{jaos solve} does: \code{.lp}, \code{.nl},
  \code{.qplib}, \code{.osil} or \code{.cbf}, with or without \code{.gz},
  and MPS for any other name.}
\item{kind}{what \code{jaos_write} writes: \code{"mps"}, \code{"lp"},
  \code{"nl"}, \code{"qplib"}, \code{"cbf"} or \code{"osil"} for the model,
  \code{"solution"} for the last answer.}
\item{cost}{the objective's coefficient of each column.}
\item{col_lower, col_upper}{each column's bounds; \code{-Inf} and \code{Inf}
  are allowed. \code{jaos_build_lp} and \code{jaos_solve_lp} also take one
  value for every column.}
\item{row_lower, row_upper}{each row's bounds; \code{-Inf} and \code{Inf}
  are allowed.}
\item{a_start, a_index, a_value}{the matrix column-wise: 0-based starts, one
  more than the columns, then the 0-based row index and the value of each
  entry.}
\item{maximize}{\code{TRUE} to maximise the objective.}
\item{offset}{the objective's constant.}
\item{cols}{1-based column indices. \code{NULL} means every column. In
  \code{jaos_set_quadratic} and \code{jaos_set_row_quadratic} it is the
  column of each entry of \code{Q}.}
\item{rows}{1-based row indices. \code{NULL} means every row. In
  \code{jaos_set_quadratic} and \code{jaos_set_row_quadratic} it is the row
  of each entry of \code{Q}: one triplet per diagonal entry and one per
  off-diagonal pair, in \eqn{\frac{1}{2} x' Q x}{1/2 x'Qx}.}
\item{row, col}{one 1-based row index and one 1-based column index.}
\item{values}{the value of each entry of \code{Q}, of each coefficient
  of the row \code{jaos_node_add_row} adds, or of each column in the point
  \code{jaos_node_add_solution} hands the tree, which the tree rounds and
  keeps as its incumbent before its next node when the point meets every
  bound and row and beats the incumbent it has.}
\item{lower, upper}{bounds: one per index for the bound setters, one per
  new column or row for \code{jaos_add_cols} and \code{jaos_add_rows}, and
  the two sides of the row \code{jaos_node_add_row} adds. One value is
  used for every index.}
\item{sense}{\code{"minimize"} or \code{"maximize"}.}
\item{value}{an option's value for \code{jaos_set_option} (a logical becomes
  \code{"true"} or \code{"false"}); a coefficient for
  \code{jaos_set_coefficient}; 0 or 1 for \code{jaos_set_row_indicator}.}
\item{A}{a matrix. It is a dense matrix, a triplet
  \code{list(i, j, x)} with 1-based row and column indices (the form of
  \code{Matrix::sparseMatrix(i, j, x)}; slam's
  \code{simple_triplet_matrix} with \code{v} is read too), a column form
  \code{list(p, i, x)} laid out as a \code{dgCMatrix} (0-based starts
  \code{p} and 0-based rows \code{i}), or a \code{dgCMatrix} or
  \code{dgTMatrix} object. No package is needed for any of them. In
  \code{jaos_add_cols} it has one row per model row and one column per new
  column; in \code{jaos_add_rows} one row per new row and one column per
  model column.}
\item{names}{a name per index; \code{NA} gives back the positional name.}
\item{name}{an option's name, one of \code{jaos_option_names()}; or the
  objective's or the model's new name.}
\item{on}{\code{TRUE} to set the mark, \code{FALSE} to clear it.}
\item{type}{1 or 2, the kind of special ordered set.}
\item{weights}{the weight of each member of a special ordered set.}
\item{k}{a 1-based cone index, or special ordered set index for
  \code{jaos_sos}. \code{jaos_delete_cones} takes several.}
\item{q}{a column's quadratic objective term, as \eqn{\frac{q}{2}
  x^2}{q/2 x^2}.}
\item{rotated}{\code{TRUE} for a rotated second-order cone.}
\item{level}{how much of the solver's log \code{jaos_set_log} delivers:
  \code{"off"}, \code{"summary"}, \code{"progress"} or \code{"detail"}.}
\item{fn}{an R function, or \code{NULL} to remove it. See the section on
  callbacks.}
\item{event}{the node event a node callback received.}
\item{x}{one value per column.}
\item{message}{the first line of an AMPL \code{.sol} file; \code{NULL} for
  the status and the objective.}
\item{row_dual}{one multiplier per row, or \code{NULL} to skip the dual
  side.}
\item{cone_dual, cone_ray}{a list of one vector per cone, as
  \code{jaos_cone_dual} returns them.}
\item{row_ray}{a Farkas multiplier per row.}
\item{col_ray}{a direction per column.}
\item{tol}{the checker's tolerance.}
\item{col_status, row_status}{a basis status per column and per row:
  \code{"basic"}, \code{"at_lower"}, \code{"at_upper"} or \code{"free"}.}
\item{iis}{what \code{jaos_iis} returned, or a list of \code{row_side} and
  \code{col_side}, each \code{"none"}, \code{"lower"}, \code{"upper"} or
  \code{"both"}.}
\item{scope}{which bounds \code{jaos_feasrelax} may move: \code{"rows"},
  \code{"cols"} or \code{"both"}.}
\item{obj}{the objective's coefficient of each column.}
\item{integer}{1-based indices of the integer columns.}
\item{Q}{the objective's quadratic part, the whole symmetric matrix in any
  form \code{A} takes, or \code{NULL}. Its lower triangle is what is read.}
\item{cones}{a list of vectors, each the 1-based columns of one
  second-order cone.}
\item{options}{a named list of options set before the solve.}
\item{rotated_cones}{a list of vectors, each the 1-based columns of one
  rotated second-order cone.}
\item{quadratic_rows}{a list of \code{list(row, Q)}: the row's quadratic
  part, the whole symmetric matrix in any form \code{A} takes.}
\item{sos}{a list of \code{list(type, cols, weights)}; the weights default
  to \code{seq_along(cols)}.}
\item{indicators}{a list of \code{list(row, col, value)}: the row holds only
  while the integer column \code{col} equals \code{value}, 1 by default.}
\item{semicontinuous}{1-based indices of the semi-continuous columns.}
\item{start}{a MIP start, one value per column.}
}
\details{
Every index is 1-based, as it is everywhere in R. A call the library refuses
is an R error that carries the library's message, and so is an array of the
wrong length. The setters return their model invisibly.
}
\section{The library}{
\code{jaos_version} is the library's version. \code{jaos_infinity} is the
library's infinity, which is \code{Inf}. \code{jaos_status_str} names a
status code. \code{jaos_model_error} is the model's message about its last
failure. \code{jaos_option_names} lists every option name.
}
\section{Building and editing a model}{
\code{jaos_model} makes an empty model. \code{jaos_copy} copies a model with
its names, settings, callbacks and starting basis, and without its answer.
\code{jaos_read} reads a file into a new model. \code{jaos_write} writes the
model in one of six formats, or the last answer as a solution file.
\code{jaos_load_lp} sets the whole model in one call.

\code{jaos_dims} is \code{c(columns, rows, cones)} and \code{jaos_num_nz}
the count of matrix entries. \code{jaos_col_cost}, \code{jaos_col_bounds}
and \code{jaos_row_bounds} read costs and bounds, and
\code{jaos_set_col_cost}, \code{jaos_set_col_bounds} and
\code{jaos_set_row_bounds} change them. \code{jaos_sense} and
\code{jaos_obj_offset} read the sense and the constant, and
\code{jaos_set_sense} and \code{jaos_set_obj_offset} change them. A change
discards the answer and keeps the basis, so the next \code{jaos_solve}
starts warm.

\code{jaos_col_entries} and \code{jaos_row_entries} read one column or one
row of the matrix. \code{jaos_coefficient} reads one entry and
\code{jaos_set_coefficient} sets one; zero deletes it. \code{jaos_add_cols}
and \code{jaos_add_rows} append columns and rows. \code{jaos_delete_cols}
and \code{jaos_delete_rows} remove a set of them, and the rest are numbered
again from 1.
}
\section{Names}{
\code{jaos_col_name} and \code{jaos_row_name} read names; a column or row
with none has its positional name, \code{C1} or \code{R1}.
\code{jaos_set_col_name} and \code{jaos_set_row_name} set them.
\code{jaos_col_index} and \code{jaos_row_index} look a name up.
\code{jaos_objective_name} and \code{jaos_model_name} read the objective's
and the model's names, and \code{jaos_set_objective_name} and
\code{jaos_set_model_name} set them.
}
\section{Integer, SOS, indicator and semi-continuous columns}{
\code{jaos_set_integer} marks columns integer and \code{jaos_integer} reads
the marks. \code{jaos_set_semicontinuous} marks columns semi-continuous: such
a column rests at zero or inside its bounds. \code{jaos_semicontinuous}
reads those marks. \code{jaos_has_integer} says whether a solve runs the
branch and bound. \code{jaos_add_sos} adds a special ordered set,
\code{jaos_num_sos} counts them and \code{jaos_sos} reads one back as
\code{list(type, cols, weights)}. \code{jaos_set_row_indicator} makes a row
hold only while an integer column has a value, and \code{NA} for the column
makes it a plain row again. \code{jaos_row_indicator} reads it back as
\code{list(col, value)}, with \code{col} \code{NA} when there is none.
}
\section{Quadratic parts and cones}{
\code{jaos_set_quadratic} sets the objective's quadratic part and
\code{jaos_set_row_quadratic} a row's. \code{jaos_quadratic} and
\code{jaos_row_quadratic} read them back as \code{list(rows, cols, values)},
the lower triangle with the diagonal, and \code{jaos_quadratic_nz} and
\code{jaos_row_quadratic_nz} count their entries.
\code{jaos_set_col_quadratic} sets one diagonal term and
\code{jaos_col_quadratic} reads it. \code{jaos_add_cone} adds a second-order
cone over columns, \code{jaos_cone} reads one back as \code{list(type,
cols)}, and \code{jaos_delete_cones} removes cones.
}
\section{Options and callbacks}{
\code{jaos_set_option} sets any option by name and \code{jaos_get_option}
reads it as text. \code{jaos_read_options} reads a file of \code{name value}
lines, with \code{#} for a comment.

\code{jaos_set_log} turns the solver's log on. Without \code{fn} the lines
go to the console's error stream; with \code{fn} each line arrives as
\code{fn(level, line)}.

\code{jaos_set_progress_callback} calls \code{fn(p)} as a solve runs, with
\code{p} a list of \code{iterations}, \code{work_units} and
\code{primal_infeasibility}. \code{jaos_set_incumbent_callback} calls
\code{fn(inc)} for each new incumbent of a branch and bound, with
\code{inc} a list of \code{node}, \code{objective}, \code{bound}, \code{x}
and \code{by_rounding}. \code{jaos_set_node_callback} calls \code{fn(ev)} at
every node once its relaxation is solved, and for every point a heuristic
would make an incumbent. \code{ev} is an environment holding \code{node},
\code{depth}, \code{objective}, \code{bound}, \code{x}, \code{integral} and
\code{branch_col}. The callback may set \code{ev$branch_col} to another
integer column that is fractional at the point, and the tree branches on
it. \code{jaos_node_add_row} adds a row through the event; the row holds for
the rest of the search. An event is over once its callback returns.

A callback that returns \code{"stop"} ends the solve as
\code{"interrupted by callback"}; anything else lets it run on. An R error
inside a callback is printed and also ends the solve that way, so no error
crosses the solver. A callback must not call JAOS on the same model. The
library calls every callback on R's own thread. When the concurrent solve
or a batch of tree nodes runs on more than one thread, the nodes and
methods on the other threads make no progress calls, so \code{fn} sees
fewer calls than on one thread.
}
\section{Solving and the answer}{
\code{jaos_solve} solves the model and returns its status invisibly. A solve
stopped by a limit resumes when \code{jaos_solve} runs again.
\code{jaos_status} says how the last solve ended: \code{"optimal"},
\code{"infeasible"}, \code{"unbounded"}, a limit, or a failure.
\code{jaos_objective} is the optimal objective and \code{jaos_solution} is
\code{list(x, row_activity, row_dual, reduced_cost)} of an optimal solve.
\code{jaos_iterations}, \code{jaos_work_units} and \code{jaos_solve_time}
count the last solve; the time is in seconds and changes from run to run.

\code{jaos_basis} is \code{list(col_status, row_status)} of the last solve,
\code{jaos_set_basis} hands the next solve its starting basis and
\code{jaos_clear_basis} makes it start cold. \code{jaos_cone_dual} is the
dual of one cone, one entry per member. \code{jaos_certificate} is the
Farkas multipliers of an infeasible model and \code{jaos_unbounded_ray} the
improving direction of an unbounded one; each is \code{NULL} when the solve
did not end that way.

\code{jaos_mip_result} is the branch-and-bound report, with its twelve
fields. \code{jaos_mip_incumbent} is the best integer point the tree holds,
as \code{list(x, objective)}, or \code{NULL}. \code{jaos_mip_pool} is the
solution pool, best first, as a list of \code{list(objective, x)}; the
option \code{mip_pool_size} sets its size. \code{jaos_set_mip_start} hands
the tree a starting point, and \code{NULL} removes it.
}
\section{Files}{
\code{jaos_write_sol_ampl} writes the \code{.sol} file an AMPL solver hands
back. \code{jaos_read_solution} reads back a solution file as
\code{list(objective, x, row_activity, row_dual, reduced_cost, col_status,
row_status)}. \code{jaos_read_certificate} reads the certificate of an
infeasible or unbounded answer as \code{list(status, ray)}, and
\code{jaos_read_cone_duals} its cone records as a list per cone.
\code{jaos_read_basis} reads the basis in a solution file.
\code{jaos_write_mps_basis} and \code{jaos_read_mps_basis} write and read an
MPS basis file. \code{jaos_write_point} writes the optimum's point, or the
values \code{x}, as \code{NAME VALUE} lines, and \code{jaos_read_point}
reads such a file. \code{jaos_write_duals} and \code{jaos_read_duals} do the
same for the row duals. \code{jaos_solution_file_status} says what a
solution file holds. Nothing read is installed in the model.
}
\section{Checkers and exact proofs}{
\code{jaos_check_solution} judges a point, and duals when given, against the
model. \code{jaos_check_conic_solution} does the same with cone duals.
\code{jaos_check_certificate} and \code{jaos_check_conic_certificate} judge a
claimed infeasibility certificate, and \code{jaos_check_ray} a claimed
unbounded ray. Each returns its report as a named list.

\code{jaos_verify} proves, or refuses to prove, the last optimum over the
rationals, and \code{jaos_verify_basis} does it for a basis handed in, with
no solve. \code{jaos_exact_col_value}, \code{jaos_exact_row_dual} and
\code{jaos_exact_objective} are the proved values, as text such as
\code{"1/3"}. \code{jaos_exact_certificate} derives the Farkas multipliers
exactly and \code{jaos_exact_row_multiplier} reads them.
\code{jaos_exact_unbounded_ray} derives an unbounded direction exactly and
\code{jaos_exact_col_direction} reads it. \code{jaos_write_proof} writes the
proof file and \code{jaos_check_proof} judges one from the model alone.
In every report an index is 1-based, and \code{NA} means none.
}
\section{Infeasibility, ranging and statistics}{
\code{jaos_iis} finds an irreducible infeasible subsystem of an infeasible
answer, as \code{list(row_side, col_side, report)}. \code{jaos_iis_model}
builds that subsystem as a model. \code{jaos_feasrelax} finds the smallest
total change to the bounds that makes the model feasible, as
\code{list(row_move, col_move, report)}; a negative move lowers a lower
side and a positive one raises an upper side.

\code{jaos_cost_ranging} is how far each cost may move with the basis
staying optimal, as \code{list(lower, upper)}. \code{jaos_rhs_ranging} and
\code{jaos_bound_ranging} do the same for the row and column bounds, as
\code{list(lower_lo, lower_hi, upper_lo, upper_hi)}.
\code{jaos_statistics} counts what the model holds and
\code{jaos_presolve_report} what presolve did on the last solve.
}
\section{One call}{
\code{jaos_solve_lp} builds, solves and reads back in one call: minimise
(or maximise) \eqn{obj'x + \frac{1}{2} x' Q x}{obj'x + 1/2 x'Qx} subject to
\code{row_lower <= A x <= row_upper} and the column bounds, with the listed
integer and semi-continuous columns, cones, rotated cones, quadratic rows,
special ordered sets and indicator rows, and a MIP start.
\code{jaos_build_lp} takes the same arguments and returns the model
unsolved, for a caller who sets a callback or a basis first.
\code{jaos_result} reads a solved model back in \code{jaos_solve_lp}'s
shape, so a model changed after \code{jaos_solve_lp} re-solves warm with
\code{jaos_solve} and reads back with it.
}
\value{
\code{jaos_solve_lp} and \code{jaos_result} return a list with the
\code{status}, the \code{model}, and \code{iterations}, \code{work_units}
and \code{solve_time}. An optimal solve adds the \code{objective}, the
values \code{x}, the \code{row_activity}, the \code{row_dual} and the
\code{reduced_cost}. When a limit stops a tree that holds an integer point,
the list carries that point's \code{objective} and \code{x}. A model with
integer structure adds the \code{pool}. The readers return a model and the
setters return their model invisibly. The other functions return what the
sections above describe.
}
\examples{
r <- jaos_solve_lp(c(1, 2), matrix(c(1, 1), 1), 1, Inf,
                   col_upper = c(4, Inf), integer = 2, maximize = FALSE)
r$status
r$objective

A <- list(i = c(1, 2, 1, 2), j = c(1, 1, 2, 2), x = c(1, 3, 2, 1))
r <- jaos_solve_lp(c(1, 1), A, c(-Inf, -Inf), c(4, 6), maximize = TRUE)
jaos_set_row_bounds(r$model, 1, -Inf, 3)
jaos_solve(r$model)
jaos_result(r$model)$x
}
