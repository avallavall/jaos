* The same model as t3_objname.mps, with OBJNAME's row name on the line
* after the header instead of on it. Both spellings must give one model.
NAME t3b
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
