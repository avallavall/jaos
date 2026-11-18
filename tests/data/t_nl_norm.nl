g3 1 1 0	# problem unknown
 3 2 1 0 0 	# vars, constraints, objectives, ranges, eqns
 1 0 0 0 0 0	# nonlinear constrs, objs; ccons: lin, nonlin, nd, nzlb
 0 0	# network constraints: nonlinear, linear
 2 0 0 	# nonlinear vars in constraints, objectives, both
 0 0 0 1	# linear network variables; functions; arith, flags
 0 0 0 0 0 	# discrete variables: binary, integer, nonlinear (b,c,o)
 5 1 	# nonzeros in Jacobian, obj. gradient
 1 1	# max name lengths: constraints, variables
 0 0 0 0 0	# common exprs: b,c,o,c1,o1
C0	#c
o39	#sqrt
o0	#+
o5	#^
v0	#x
n2
o5	#^
v1	#y
n2
C1	#l
n0
O0 0	#o
n0
x0	# initial guess
r	#2 ranges (rhs's)
1 0	#c
2 2	#l
b	#3 bounds (on variables)
3	#x
3	#y
3	#t
k2	#intermediate Jacobian column lengths
2
4
J0 3	#c
0 0
1 0
2 -1
J1 2	#l
0 1
1 1
G0 1	#o
2 1
