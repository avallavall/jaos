# 02-296 — CBLIB's filterdesign, and active rows in the Newton finish

Taken on 2026-09-22 on the tree of 8876091 (the reading) and ba968c0
(the refusal), for TODO row C5.

## The filterdesign instances

`bench/cblib.manifest` leaves out the twelve filterdesign instances of
CBLIB 2014, 71 to 872 MB gzipped, and row C5 read "not read at all".
The smallest, `2013_firL1Linfeps.cbf.gz` (74012602 bytes, sha256
6953e87fdc6bf7b7edd418fdf6af7137448b148cb0c589c1f9215bb42b09dda1 from
cblib.zib.de/download/cblib2014/cont/), reads in 3.4 s and 1.15 GB:
30085 rows, 59173 columns, 9873426 nonzeros and 19724 cones of three
members.

It solves. `jaos solve ... --work-limit 4000000000000 --check`
(`firL1Linfeps.txt`): `OPTIMAL` at -0.015254948786391173 after 109 conic
iterations, 3.6e12 work units, 59 minutes and 2.75 GB of memory, with the
checker taking both sides (rows 2.6e-17 of their traffic, duals 1.7e-18,
cones 2.8e-17, the gap 3.6e-18). CBLIB's own table gives
-0.01525493991336033, 5.8e-7 above ours in relative terms, from MOSEK.

One conic iteration costs 3.3e10 work units and 35 s here. The other
eleven files are 72 to 872 MB, so they stay out of `make cblib`, which
reads 29 instances in about six minutes.

## Active rows in the Newton finish

`conic-qc-active-rows.diff` takes every row within 1e-6 of a side as
active in the conic walk's Newton finish, lets it run six steps instead
of two, and adds to the active set every row the finish leaves outside
its bound by more than the primal tolerance.

QPLIB_2482 (`LCD`, 1806 columns) at 1e10 work units: the finish runs on
1683 active constraints where it ran on 1682, and its KKT residual rises
from 4.669e-07 to 2.276e-06 at the first step, so the finish keeps its
old point and the duals stay 8.5e-7 off. QPLIB_3088 and QPLIB_2456 read
the same as before, their finishes refused at 3.7e-3. The active-set
update never runs, because the step it would follow is never taken.
Refused (`conic-qc-active-rows`).
