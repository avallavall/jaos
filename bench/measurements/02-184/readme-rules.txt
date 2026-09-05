# two candidate record-check rules, measured before either is added
# tree: 16ce733
# measurement directories tracked: 179

RULE A -- a README heading naming another directory
  judged:  123
  skipped: 56 (heading does not name an id; predates the convention)
  FIRES:   0

RULE B -- a reading committed after the README that names it
  README/.txt pairs judged: 227

  B-raw, any commit:   5 fire
    02-125   unbounded.txt
    02-134   assert-control.txt
    02-179   proofs-netlib.txt
    02-179   proofs.txt
    02-73    fold-and-chain.txt

  B-data, a non-comment line moved: 3 fire
    02-134   assert-control.txt          1.0 h newer, in a89a355
    02-179   proofs-netlib.txt           0.2 h newer, in 499c142
    02-73    fold-and-chain.txt          0.7 h newer, in cd68630

  filtered out as header-only: 2
