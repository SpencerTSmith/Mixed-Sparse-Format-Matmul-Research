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

from plot import *

import numpy as np

def diagonal(size, diag_density=0.8, noise_density=0.05, seed=42):
    rng = np.random.default_rng(seed)

    matrix = np.zeros((size,size))

    for i in range(size):
        for b in range(-16,17):
            j = i + b
            if j >= 0 and j < size and rng.random() < diag_density:
                matrix[i, j] = rng.random()

    extra = int(noise_density * size * size)

    rows = rng.integers(0, size, extra)
    cols = rng.integers(0, size, extra)

    matrix[rows, cols] = rng.random(extra)

    return matrix

def block_densities(matrix, block_rows, block_cols):
    M, N = matrix.shape

    densities = {}

    for i, row_start, in enumerate(range(0,M,block_rows)):
        for j, col_start in enumerate(range(0,N,block_cols)):
            block = matrix[row_start:row_start+block_rows,col_start:col_start+block_cols]

            d = np.count_nonzero(block) / block.size * 10
            densities[i,j] = int(d)

    return densities

# We are going to encode are formats as follows:
#   0 ---> dd
#   1 ---> ds
#   2 ---> sd
min_format = 0
max_format = 2

def dummy_csr_csc(LRC, LCC, RCC, LNZ, RNZ):
    flops = 2 ** 32
    memops = 2 ** 32
    return flops, memops

FORMULA_MAP = {
    (0, 0): formula_dense_dense,
    (0, 1): formula_dense_csr,
    (0, 2): formula_dense_csc,
    (1, 0): formula_csr_dense,
    (1, 1): formula_csr_csr,
    (1, 2): dummy_csr_csc,
    (2, 0): formula_csc_dense,
    (2, 1): formula_csc_csr,
    (2, 2): formula_csc_csc,
}

LRC = 16
LCC = 8
RCC = 16

MATRIX_SIZE = 128
M = MATRIX_SIZE // LRC
N = MATRIX_SIZE // RCC
K = MATRIX_SIZE // LCC

# Create a simple model
model = ConcreteModel()

model.i_range = RangeSet(0,M-1)
model.j_range = RangeSet(0,N-1)
model.p_range = RangeSet(0,K-1)
model.sparse_formats_range = RangeSet(min_format,max_format)

model.density_range = RangeSet(0,9)

def density_to_nnz(density_idx, rows, cols):
    return int(density_idx / 10 * rows * cols)

costs = {}
for fa in range(min_format, max_format + 1):
    for fb in range(min_format, max_format + 1):
        for dA in range(0, 10):
            for dB in range(0, 10):
                LRC = 16
                LCC = 8
                RCC = 16
                LNZ = density_to_nnz(dA, LRC, LCC)
                RNZ = density_to_nnz(dB, LCC, RCC)
                flops, memops = FORMULA_MAP[fa, fb](LRC, LCC, RCC, LNZ, RNZ)
                costs[fa, fb, dA, dB] = flops + memops

# Densities
# These would be the actual densities of each block of A and B
densityA=block_densities(diagonal(MATRIX_SIZE), LRC, LCC)

densityB=block_densities(diagonal(MATRIX_SIZE), LCC, RCC)

##############
# Parameters #
##############

model.costs = Param(model.sparse_formats_range,
                    model.sparse_formats_range,
                    model.density_range,
                    model.density_range,
                    initialize=costs, default=100)
# model.costs.display()

model.densityA = Param(model.i_range, model.p_range, initialize=densityA, default=0)
# model.densityA.display()

model.densityB = Param(model.p_range, model.j_range, initialize=densityB, default=0)
# model.densityB.display()


#############
# Variables #
#############
model.a_format = Var(model.i_range,model.p_range,model.sparse_formats_range,within=NonNegativeIntegers, bounds=(0,1))
# model.a_format.display()

model.b_format = Var(model.p_range,model.j_range,model.sparse_formats_range,within=NonNegativeIntegers, bounds=(0,1))
# model.b_format.display()

################
# Constraints  #
################


# Each block can only be 1 format at a time
model.c_range_check_a = ConstraintList()
for i in model.i_range:
    for p in model.p_range:
        model.c_range_check_a.add(sum(model.a_format[i,p,fa] for fa in model.sparse_formats_range) == 1)


# model.c_range_check_a.display()


model.c_range_check_b = ConstraintList()
for p in model.p_range:
    for j in model.j_range:
        model.c_range_check_b.add(sum(model.b_format[p,j,fb] for fb in model.sparse_formats_range) == 1)


# model.c_range_check_b.display()




##############
# Objective  #
##############
# The runtime is the sum of the runtimes of all block multiplications.
# The cost for the individual multiplies is dependent on the formats and densities of A and B
# which is encoded in both the expression and how we map the format assignment to A and B

model.total_time = sum(model.costs[fa,fb,model.densityA[i,p],model.densityB[p,j]]*model.a_format[i,p,fa]*model.b_format[p,j,fb]
                       for fa in model.sparse_formats_range
                       for fb in model.sparse_formats_range
                       for i in model.i_range for j in model.j_range for p in model.p_range)


model.objective = Objective(rule=model.total_time, sense=minimize)
# model.objective.display()

# Solve the model using MindtPy
SolverFactory('mindtpy').solve(model, mip_solver='glpk', nlp_solver='ipopt')

print("======= DONE ========")
print("= LOOK at the assignments of a_format and b_format")

# model.objective.display()
model.display()
model.pprint()

print(value(model.total_time))

format_names = {0: "dense", 1: "CSR", 2: "CSC"}

print("\n=== Optimal A formats ===")
for i in model.i_range:
    for p in model.p_range:
        chosen = next(fa for fa in model.sparse_formats_range if value(model.a_format[i,p,fa]) > 0.5)
        print(f"  A[{i},{p}] (density={densityA[i,p]*10}%) -> {format_names[chosen]}")

print("\n=== Optimal B formats ===")
for p in model.p_range:
    for j in model.j_range:
        chosen = next(fb for fb in model.sparse_formats_range if value(model.b_format[p,j,fb]) > 0.5)
        print(f"  B[{p},{j}] (density={densityB[p,j]*10}%) -> {format_names[chosen]}")

print(f"\n=== Total cost: {value(model.total_time):.0f} ===")
