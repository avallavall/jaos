\name{jaos}
\alias{jaos}
\alias{jaos_version}
\alias{jaos_model}
\alias{jaos_read}
\alias{jaos_write}
\alias{jaos_load_lp}
\alias{jaos_set_integer}
\alias{jaos_set_quadratic}
\alias{jaos_set_row_quadratic}
\alias{jaos_add_cone}
\alias{jaos_set_option}
\alias{jaos_get_option}
\alias{jaos_solve}
\alias{jaos_status}
\alias{jaos_objective}
\alias{jaos_dims}
\alias{jaos_solution}
\alias{jaos_cone_dual}
\alias{jaos_certificate}
\alias{jaos_unbounded_ray}
\alias{jaos_mip_result}
\alias{jaos_mip_incumbent}
\alias{jaos_set_log}
\alias{jaos_solve_lp}
\title{The JAOS Solver: Linear, Mixed-Integer, Quadratic and Conic Programs}
\description{
The JAOS C library from R. A model is read from an MPS, LP, NL, QPLIB, OSiL
or CBF file, or built from vectors, then solved, and its values, duals,
certificates and branch-and-bound report are read back. Every answer is
bit-identical on every machine and is judged by an independent checker.
}
\usage{
jaos_version()
jaos_model()
jaos_read(path)
jaos_write(model, path, kind = "mps")
jaos_load_lp(model, cost, col_lower, col_upper, row_lower, row_upper,
             a_start, a_index, a_value, maximize = FALSE, offset = 0)
jaos_set_integer(model, cols)
jaos_set_quadratic(model, rows, cols, values)
jaos_set_row_quadratic(model, row, rows, cols, values)
jaos_add_cone(model, cols, rotated = FALSE)
jaos_set_option(model, name, value)
jaos_get_option(model, name)
jaos_solve(model)
jaos_status(model)
jaos_objective(model)
jaos_dims(model)
jaos_solution(model)
jaos_cone_dual(model, k)
jaos_certificate(model)
jaos_unbounded_ray(model)
jaos_mip_result(model)
jaos_mip_incumbent(model)
jaos_set_log(model, level = "summary")
jaos_solve_lp(obj, A, row_lower, row_upper,
              col_lower = rep(0, length(obj)),
              col_upper = rep(Inf, length(obj)),
              integer = NULL, Q = NULL, cones = list(),
              maximize = FALSE, offset = 0, options = list())
}
\arguments{
\item{path}{a file name. \code{jaos_read} picks the reader by its
  extension, as \code{jaos solve} does: \code{.lp}, \code{.nl},
  \code{.qplib}, \code{.osil} or \code{.cbf}, with or without \code{.gz},
  and MPS for any other name.}
\item{model}{a model, from \code{jaos_model} or \code{jaos_read}.}
\item{kind}{what \code{jaos_write} writes: \code{"mps"} or \code{"lp"} for
  the model, \code{"solution"} for the last answer.}
\item{cost}{the objective's coefficient for each column.}
\item{col_lower, col_upper}{each column's bounds; \code{-Inf} and \code{Inf}
  are allowed.}
\item{row_lower, row_upper}{each row's bounds; \code{-Inf} and \code{Inf}
  are allowed.}
\item{a_start, a_index, a_value}{the matrix column-wise: 0-based starts, one
  more than the columns, then the 0-based row index and the value of each
  entry.}
\item{maximize}{\code{TRUE} to maximise the objective.}
\item{offset}{the objective's constant.}
\item{cols}{1-based column indices.}
\item{rows}{1-based column indices paired with \code{cols}: each triplet is
  one entry of \code{Q}, one per diagonal entry and one per off-diagonal
  pair, in the objective's \eqn{\frac{1}{2} x' Q x}{1/2 x'Qx}.}
\item{values}{the value of each entry of \code{Q}.}
\item{row}{the 1-based row whose quadratic part \code{jaos_set_row_quadratic}
  sets.}
\item{rotated}{\code{TRUE} for a rotated second-order cone.}
\item{name}{an option's name, one of those \code{jaos options} lists.}
\item{value}{the option's value; a logical becomes \code{"true"} or
  \code{"false"}.}
\item{k}{a 1-based cone index.}
\item{level}{how much of the solver's log \code{jaos_set_log} prints to the
  console's error stream: \code{"off"}, \code{"summary"},
  \code{"progress"} or \code{"detail"}.}
\item{obj}{the objective's coefficient for each column.}
\item{A}{the constraint matrix, dense.}
\item{integer}{1-based indices of the integer columns.}
\item{Q}{the objective's quadratic part, a symmetric dense matrix, or
  \code{NULL}.}
\item{cones}{a list of vectors, each the 1-based columns of one
  second-order cone.}
\item{options}{a named list of options set before the solve.}
}
\details{
\code{jaos_read} reads a file into a new model. \code{jaos_load_lp} sets the
whole model in one call. \code{jaos_set_integer} marks columns integer.
\code{jaos_set_quadratic} sets the objective's quadratic part and
\code{jaos_set_row_quadratic} a row's. \code{jaos_add_cone} adds a
second-order cone over columns. \code{jaos_solve} solves the model, and
\code{jaos_status} says how the solve ended: \code{"optimal"},
\code{"infeasible"}, \code{"unbounded"}, a limit, or a failure.

\code{jaos_dims} is \code{c(columns, rows, cones)}.
\code{jaos_solution} is \code{list(x, row_activity, row_dual,
reduced_cost)} of an optimal solve. \code{jaos_cone_dual} is the dual of
one cone, one entry per member. \code{jaos_certificate} is the Farkas
multipliers of an infeasible model and \code{jaos_unbounded_ray} the
improving direction of an unbounded one; each is \code{NULL} when the
solve did not end that way. \code{jaos_mip_result} is the
branch-and-bound report, and \code{jaos_mip_incumbent} the best integer
point the tree holds, as a list of \code{x} and \code{objective}, or
\code{NULL} when it holds none; it is there after a limit stops the tree.
\code{jaos_set_log} prints the solver's log lines during a solve.

\code{jaos_solve_lp} does all of it in one call from a dense matrix:
minimise (or maximise) \eqn{obj'x + \frac{1}{2} x' Q x}{obj'x + 1/2 x'Qx}
subject to \code{row_lower <= A x <= row_upper} and the column bounds,
with the listed columns integer and each element of \code{cones} a
second-order cone.

A call the library refuses is an R error carrying the library's message.
}
\value{
\code{jaos_solve_lp} returns a list with the \code{status} and the
\code{model}, and for an optimal solve also the \code{objective}, the
values \code{x}, the \code{row_dual} and the \code{reduced_cost}. When a
limit stops a tree that holds an integer point, the list carries that
point's \code{objective} and \code{x}. The readers return a model; the
setters return their model invisibly.
}
\examples{
r <- jaos_solve_lp(c(1, 2), matrix(c(1, 1), 1), 1, Inf,
                   col_upper = c(4, Inf), integer = 2, maximize = FALSE)
r$status
r$objective
}
