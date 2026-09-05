# D279 -- does the README-id check catch a heading pointing elsewhere?
# date: 2026-09-05T10:42:43Z
# tree: 16ce733
# victim: bench/measurements/02-182/README.md, heading rewritten to name 02-999

== arm 0: the tree as it stands
record-check: PASS
exit=0

== arm 1: the heading names another directory
# 02-999 — the checker's dual half in `double`, and what moved
  FAIL  bench/measurements/02-182/README.md names 02-999 in its first heading, and it is 02-182's README. A measurement directory is one decision's evidence; if this one was moved, repoint the heading, and if a second decision wrote into it, give that one its own id
exit=1

== arm 2: restored
# 02-182 — the checker's dual half in `double`, and what moved
record-check: PASS
exit=0

the check catches it, names it, and goes quiet again
verdict-exit=0
