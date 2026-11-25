# 02-511 — the clique separator extends a clique over its first literal's neighbours

Taken on 2026-11-25 on the tree of 02-510, for J36's item on the seconds
the separators take that the work does not bill. In 02-501's split the
separators bill 1.1% of the root's work and take 4.1% of its seconds, and
more than half of the 300 s on `neos-4722843-widden`,
`neos-2746589-doon`, `neos-3555904-turama` and `radiationm40-10-02`.

## The cause

`clique_round` sorts the literals of the clique table's edges by their
value at the relaxation's point and, from each start, grows a clique
greedily: it walks every other literal and takes one that is adjacent to
every member, each test a binary search in the edge list. A start costs a
pass over all the literals however small the clique it builds, and the
work bills only the members' squares. Each cut was then written into a
dense vector of the columns, cleared first and scanned twice by
`cutbuf_push`.

## The change

A literal joins a clique only if it is adjacent to the first member, so
the walk now visits the first member's neighbours from the clique table's
adjacency lists, sorted by their place in the sorted literals: the same
literals in the same order, and the same cliques. A cut is written from
its members' columns, sorted, and cleared by them. The work billed is
unchanged, so every answer and work unit is the same.

## The reading

Identity: MIPLIB 2017's 30 at 5e8 work units read the same in every line
but `time`, the digests included (`miplib2017-identity.txt`), and MIPLIB
3 does not move (`miplib3-read.txt`).

Seconds at 3e9 work units, both builds with 02-501's split (`split510.py`)
at once (`work3e9-read.txt`), the logs equal but for times:

| instance | clique separator, s | the solve, s |
|---|---|---|
| `neos-4722843-widden` | 295.8 to 0.29 | 324.3 to 28.5 |
| `neos-2746589-doon` | 83.5 to 0.28 | 105.4 to 23.2 |
| `neos-3555904-turama` | 53.9 to 0.11 | 69.3 to 17.5 |
| `radiationm40-10-02` | 0 | 18.5 to 17.8 |

`radiationm40-10-02`'s clique rounds come after 3e9.

## The 15 at 300 s

`pair.sh` runs 02-501's `pair.sh` on the 15 where 02-501's separators
take 3% of the seconds or more (`sep15.txt`), 02-510's tree against the
change, both with the round lines, two legs at a time, load 8 to 11
(`pair-record.txt`, `pairsum.txt`). Every path is the same, so a leg only
gets further:

| instance | work in 300 s, before | after | nodes | |
|---|---|---|---|---|
| `neos-4722843-widden` | 2.5e9 | 3.3e10 | 1 to 187 | incumbent 64888, bound 6275 to 10977 |
| `neos-2746589-doon` | 3.5e9 | 7.0e10 | 1 to 34 | |
| `neos-3555904-turama` | 2.2e10 | 7.8e10 | 1 | |
| `chromaticindex512-7` | 9.4e9 | 3.5e10 | 1 | |
| `netdiversion` | 1.6e10 | 4.4e10 | 1 | |
| `bab6` | 2.1e10 | 5.7e10 | 1 | bound -293081 to -291744 |
| `radiationm40-10-02` | 2.1e10 | 3.8e10 | 1 | |
| `physiciansched6-2` | 2.0e10 | 3.1e10 | 1 | |

Two leave the root, and `fiball`'s incumbent goes from 152 to 145 at the
same 101 nodes. The other seven reach within 1.25x of the before leg's work.

## The gates

`make test` and `make sanitize` pass, and every record of `bench/results/`
that `netlib`, `netlib-infeas`, `netlib-kennington`, `miplib`, `primal
barrier pdlp concurrent warm` and `miplib2017` write is byte-identical to
the committed one.
