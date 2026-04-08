#!/usr/bin/env python3
#
# https://pyomo.readthedocs.io/en/6.8.0/contributed_packages/mindtpy.html
#
# On fedora 42
# sudo dnf install glpk
# sudo dnf install glpk-utils
# sudo dnf install glpk-devel
# sudo dnf install ipopt

from pyomo.environ import *

# We are going to encode are formats as follows:
#   0 ---> dd
#   1 ---> ds
#   2 ---> sd
#   3 ---> ss
#
min_format = 0
max_format = 3

M=2
N=2
K=2

# Create a simple model
model = ConcreteModel()

model.i_range = RangeSet(0,M-1)
model.j_range = RangeSet(0,N-1)
model.p_range = RangeSet(0,K-1)
model.sparse_formats_range = RangeSet(min_format,max_format)


model.density_range = RangeSet(1,9) # 10% -- 90%

# These are the costs associated with multiplying two formats together.
# The numbers are fudged here, and should be made real.
#
#
#   Example costs[0,1,8,2] = 23 means if format of A = 0 (dd) and B =1 (ds)
#                            and the density of A is 80% and the density of
#                            B is 20% then the runtime is 5 units.
#
costs={}
costs[0,0,1,2]=51
costs[0,0,1,7]=52
costs[0,0,8,2]=53
costs[0,0,8,7]=543
costs[0,1,1,2]=21
costs[0,1,1,7]=22
costs[0,1,8,2]=23
costs[0,1,8,7]=24
costs[0,2,1,2]=313
costs[0,2,1,7]=32
costs[0,2,8,2]=33
costs[0,2,8,7]=343
costs[0,3,1,2]=41
costs[0,3,1,7]=42
costs[0,3,8,2]=43
costs[0,3,8,7]=44
costs[1,0,1,2]=413
costs[1,0,1,7]=42
costs[1,0,8,2]=433
costs[1,0,8,7]=44
costs[1,1,1,2]=31
costs[1,1,1,7]=32
costs[1,1,8,2]=33
costs[1,1,8,7]=343
costs[1,2,1,2]=21
costs[1,2,1,7]=22
costs[1,2,8,2]=23
costs[1,2,8,7]=24
costs[1,3,1,2]=11
costs[1,3,1,7]=12
costs[1,3,8,2]=13
costs[1,3,8,7]=143
costs[2,0,1,2]=31
costs[2,0,1,7]=32
costs[2,0,8,2]=33
costs[2,0,8,7]=34
costs[2,1,1,2]=21
costs[2,1,1,7]=223
costs[2,1,8,2]=23
costs[2,1,8,7]=24
costs[2,2,1,2]=11
costs[2,2,1,7]=123
costs[2,2,8,2]=13
costs[2,2,8,7]=14
costs[2,3,1,2]=41
costs[2,3,1,7]=42
costs[2,3,8,2]=433
costs[2,3,8,7]=44
costs[3,0,1,2]=11
costs[3,0,1,7]=12
costs[3,0,8,2]=13
costs[3,0,8,7]=14
costs[3,1,1,2]=41
costs[3,1,1,7]=423
costs[3,1,8,2]=43
costs[3,1,8,7]=44
costs[3,2,1,2]=213
costs[3,2,1,7]=22
costs[3,2,8,2]=233
costs[3,2,8,7]=24
costs[3,3,1,2]=313
costs[3,3,1,7]=323
costs[3,3,8,2]=33
costs[3,3,8,7]=34


# Densities
# These would be the actual densities of each block of A and B
densityA={}
densityA[0,0]=8
densityA[0,1]=1
densityA[1,0]=1
densityA[1,1]=8

densityB={}
densityB[0,0]=2
densityB[0,1]=7
densityB[1,0]=7
densityB[1,1]=2


##############
# Parameters #
##############

model.costs = Param(model.sparse_formats_range,
                    model.sparse_formats_range,
                    model.density_range,
                    model.density_range,
                    initialize=costs, default=100)
model.costs.display()

model.densityA = Param(model.i_range, model.p_range, initialize=densityA, default=0)
model.densityA.display()

model.densityB = Param(model.p_range, model.j_range, initialize=densityB, default=0)
model.densityB.display()


#############
# Variables #
#############
model.a_format = Var(model.i_range,model.p_range,model.sparse_formats_range,within=NonNegativeIntegers, bounds=(0,1))
model.a_format.display()

model.b_format = Var(model.p_range,model.j_range,model.sparse_formats_range,within=NonNegativeIntegers, bounds=(0,1))
model.b_format.display()

################
# Constraints  #
################


# Each block can only be 1 format at a time
model.c_range_check_a = ConstraintList()
for i in model.i_range:
    for p in model.p_range:
        model.c_range_check_a.add(sum(model.a_format[i,p,fa] for fa in model.sparse_formats_range) == 1)


model.c_range_check_a.display()
#


model.c_range_check_b = ConstraintList()
for p in model.p_range:
    for j in model.j_range:
        model.c_range_check_b.add(sum(model.b_format[p,j,fb] for fb in model.sparse_formats_range) == 1)


model.c_range_check_b.display()




##############
# Objective  #
##############
# The runtime is the sum of the runtimes of all block multiplications.
# The cost for the individual multiplies is dependent on the formats and densities of A and B
# which is encoded in both the expression and how we map the format assignment to A and B

model.total_time = sum(model.costs[fa,fa,model.densityA[i,p],model.densityB[p,j]]*model.a_format[i,p,fa]*model.b_format[p,j,fb]
                       for fa in model.sparse_formats_range
                       for fb in model.sparse_formats_range
                       for i in model.i_range for j in model.j_range for p in model.p_range)


model.objective = Objective(rule=model.total_time, sense=minimize)
model.objective.display()

# Solve the model using MindtPy
SolverFactory('mindtpy').solve(model, mip_solver='glpk', nlp_solver='ipopt')

print("======= DONE ========")
print("= LOOK at the assignments of a_format and b_format")

model.objective.display()
model.display()
model.pprint()

print(value(model.total_time))
