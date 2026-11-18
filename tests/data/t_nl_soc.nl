g3 1 1 0	# problem unknown
 3 2 1 0 0 	# vars, constraints, objectives, ranges, eqns
 1 0 0 0 0 0	# nonlinear constrs, objs; ccons: lin, nonlin, nd, nzlb
 0 0	# network constraints: nonlinear, linear
 3 0 0 	# nonlinear vars in constraints, objectives, both
 0 0 0 1	# linear network variables; functions; arith, flags
 0 0 0 0 0 	# discrete variables: binary, integer, nonlinear (b,c,o)
 5 1 	# nonzeros in Jacobian, obj. gradient
 1 1	# max name lengths: constraints, variables
 0 0 0 0 0	# common exprs: b,c,o,c1,o1
C0	#c
o54	# sumlist
3	# (n)
o5	#^
v1	#x
n2
o5	#^
v2	#y
n2
o16	#-
o5	#^
v0	#t
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
2 0	#t
3	#x
3	#y
k2	#intermediate Jacobian column lengths
1
3
J0 3	#c
0 0
1 0
2 0
J1 2	#l
1 1
2 1
G0 1	#o
0 1
