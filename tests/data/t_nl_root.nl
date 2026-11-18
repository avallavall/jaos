g3 1 1 0	# problem unknown
 3 1 1 0 0 	# vars, constraints, objectives, ranges, eqns
 1 0 0 0 0 0	# nonlinear constrs, objs; ccons: lin, nonlin, nd, nzlb
 0 0	# network constraints: nonlinear, linear
 2 0 0 	# nonlinear vars in constraints, objectives, both
 0 0 0 1	# linear network variables; functions; arith, flags
 0 0 0 0 0 	# discrete variables: binary, integer, nonlinear (b,c,o)
 3 1 	# nonzeros in Jacobian, obj. gradient
 1 1	# max name lengths: constraints, variables
 0 0 0 0 0	# common exprs: b,c,o,c1,o1
C0	#c
o16	#-
o39	#sqrt
o2	#*
v0	#x
v1	#y
O0 1	#o
n0
x0	# initial guess
r	#1 ranges (rhs's)
1 0	#c
b	#3 bounds (on variables)
0 0 4	#x
0 0 9	#y
3	#t
k2	#intermediate Jacobian column lengths
1
2
J0 3	#c
0 0
1 0
2 1
G0 1	#o
2 1
