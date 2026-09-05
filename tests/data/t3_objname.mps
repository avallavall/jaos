* Golden instance 3: OBJNAME picks the SECOND free row, so the first one
* becomes an ordinary free row rather than the objective. The name is on the
* header line here; t3_objname_nextline.mps has it on the line after.
*
* Minimize  profit = 3x + 1y      (the row OBJNAME names)
* subject to  balance:  x + 2y = 4
*             ignored:  10x + 20y   free, both bounds infinite
*
* Read with the first-N-row rule instead, the objective would be `ignored`
* and `profit` would be a free row -- a different model with the same text.
NAME t3
OBJNAME
    PROFIT
ROWS
 N IGNORED
 N PROFIT
 E BALANCE
COLUMNS
 X PROFIT 3.0 BALANCE 1.0
 X IGNORED 10.0
 Y PROFIT 1.0 BALANCE 2.0
 Y IGNORED 20.0
RHS
 R BALANCE 4.0
ENDATA
