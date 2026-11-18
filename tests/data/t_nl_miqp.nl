g3 1 1 0	# problem unknown
 4 1 1 0 0 	# vars, constraints, objectives, ranges, eqns
 1 1 0 0 0 0	# nonlinear constrs, objs; ccons: lin, nonlin, nd, nzlb
 0 0	# network constraints: nonlinear, linear
 1 2 1 	# nonlinear vars in constraints, objectives, both
 0 0 0 1	# linear network variables; functions; arith, flags
 1 1 0 0 1 	# discrete variables: binary, integer, nonlinear (b,c,o)
 3 4 	# nonzeros in Jacobian, obj. gradient
 1 1	# max name lengths: constraints, variables
 0 0 0 0 0	# common exprs: b,c,o,c1,o1
C0	#c
o5	#^
v0	#y
n2
O0 0	#o
o0	#+
o5	#^
o0	#+
v1	#x
n-1.4
n2
o5	#^
o0	#+
v0	#y
n-0.3
n2
x0	# initial guess
r	#1 ranges (rhs's)
1 7	#c
b	#4 bounds (on variables)
0 -5 5	#y
0 -5 5	#x
0 0 1	#b
0 0 9	#z
k3	#intermediate Jacobian column lengths
1
1
2
J0 3	#c
0 0
2 -3
3 1
G0 4	#o
0 0
1 0
2 -1
3 -1
