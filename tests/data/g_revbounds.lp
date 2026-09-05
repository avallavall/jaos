\ A bound written with the value first, both ways round. `10 >= x` is
\ `x <= 10` and `8 >= y >= 2` is `2 <= y <= 8`; the third statement is the
\ form that already worked and is here as the control in the same file.
Minimize
 obj: x + y + z + w
Subject To
 c1: x + y + z + w >= 1
Bounds
 10 >= x
 8 >= y >= 2
 1 <= z <= 5
 w free
End
