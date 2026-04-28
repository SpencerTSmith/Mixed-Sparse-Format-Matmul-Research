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
import struct
import json, ast, os

NUM_DENSITY_BINS = 3
BIN_REPRESENTATIVE_DENSITY = {0: 0.1, 1: 0.7, 2: 0.9}
BIN_LABELS = {0: "(<10%)", 1: "(10-70%)", 2: "(>70%)"}

def density_bin(frac):
    if frac <= 0.1:   return 0
    elif frac <= 0.7: return 1
    else:              return 2

def block_densities(matrix, block_rows, block_cols):
    M, N = matrix.shape
    densities = {}
    for i, row_start in enumerate(range(0, M, block_rows)):
        for j, col_start in enumerate(range(0, N, block_cols)):
            block = matrix[row_start:row_start+block_rows, col_start:col_start+block_cols]
            densities[i,j] = density_bin(np.count_nonzero(block) / block.size)
    return densities

def density_to_nnz(density_idx, rows, cols):
    return int(density_idx / 10 * rows * cols)

def dump_matrix(f, matrix):
    rows, cols = matrix.shape
    f.write(struct.pack('II', rows, cols))
    # row-major f64
    f.write(matrix.astype(np.float64).tobytes())

def make_diagonal(size, diag_density=0.8, noise_density=0.05, seed=42):
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

# We are going to encode are formats as follows:
#   0 ---> dd
#   1 ---> sd
#   2 ---> ds
#   2 ---> ss
min_format = 0
max_format = 3

FORMULA_MAP = {
    (0, 0): formula_dense_dense,
    (0, 1): formula_dense_csr,
    (0, 2): formula_dense_csc,
    (0, 3): formula_dense_coo,
    (1, 0): formula_csr_dense,
    (1, 1): formula_csr_csr,
    (1, 2): formula_csr_csc,
    (1, 3): formula_csr_coo,
    (2, 0): formula_csc_dense,
    (2, 1): formula_csc_csr,
    (2, 2): formula_csc_csc,
    (2, 3): formula_csc_coo,
    (3, 0): formula_coo_dense,
    (3, 1): formula_coo_csr,
    (3, 2): formula_coo_csc,
    (3, 3): formula_coo_coo,
}

LRC = 32
LCC = 32
RCC = 32

MATRIX_SIZE = 256
M = MATRIX_SIZE // LRC
N = MATRIX_SIZE // RCC
K = MATRIX_SIZE // LCC

# Create a simple model
model = ConcreteModel()

model.i_range = RangeSet(0,M-1)
model.j_range = RangeSet(0,N-1)
model.p_range = RangeSet(0,K-1)
model.sparse_formats_range = RangeSet(min_format,max_format)

model.density_range = RangeSet(0, NUM_DENSITY_BINS - 1)

if True:
    with open("runtime_sweep/costs_cache.json") as f:
        raw = json.load(f)
    costs = {ast.literal_eval(k): v for k, v in raw.items()}
else:
    costs = {}
    for fa in range(min_format, max_format + 1):
        for fb in range(min_format, max_format + 1):
            for dA in range(NUM_DENSITY_BINS):
                for dB in range(NUM_DENSITY_BINS):
                    LNZ = int(BIN_REPRESENTATIVE_DENSITY[dA] * LRC * LCC)
                    RNZ = int(BIN_REPRESENTATIVE_DENSITY[dB] * LCC * RCC)
                    flops, memops = FORMULA_MAP[fa, fb](LRC, LCC, RCC, LNZ, RNZ)
                    costs[fa, fb, dA, dB] = flops + (10 * memops)

matrixA = make_diagonal(MATRIX_SIZE)
matrixB = make_diagonal(MATRIX_SIZE)

# Densities
# These would be the actual densities of each block of A and B
densityA=block_densities(matrixA, LRC, LCC)

densityB=block_densities(matrixB, LCC, RCC)

##############
# Parameters #
##############

model.costs = Param(model.sparse_formats_range,
                    model.sparse_formats_range,
                    model.density_range,
                    model.density_range,
                    initialize=costs, default=100)

model.densityA = Param(model.i_range, model.p_range, initialize=densityA, default=0)

model.densityB = Param(model.p_range, model.j_range, initialize=densityB, default=0)


#############
# Variables #
#############
model.a_format = Var(model.i_range,model.p_range,model.sparse_formats_range,within=NonNegativeIntegers, bounds=(0,1))

model.b_format = Var(model.p_range,model.j_range,model.sparse_formats_range,within=NonNegativeIntegers, bounds=(0,1))

################
# Constraints  #
################


# Each block can only be 1 format at a time
model.c_range_check_a = ConstraintList()
for i in model.i_range:
    for p in model.p_range:
        model.c_range_check_a.add(sum(model.a_format[i,p,fa] for fa in model.sparse_formats_range) == 1)

model.c_range_check_b = ConstraintList()
for p in model.p_range:
    for j in model.j_range:
        model.c_range_check_b.add(sum(model.b_format[p,j,fb] for fb in model.sparse_formats_range) == 1)

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
SolverFactory('mindtpy').solve(model, mip_solver='glpk', nlp_solver='ipopt', tee=True)

print(value(model.total_time))

format_names = {0: "dense", 1: "CSR", 2: "CSC", 3: "COO"}

print("\n=== Optimal A formats ===")
for i in model.i_range:
    for p in model.p_range:
        chosen = next(fa for fa in model.sparse_formats_range if value(model.a_format[i,p,fa]) > 0.5)
        print(f"  A[{i},{p}] (density={BIN_LABELS[densityA[i,p]]}) -> {format_names[chosen]}")

print("\n=== Optimal B formats ===")
for p in model.p_range:
    for j in model.j_range:
        chosen = next(fb for fb in model.sparse_formats_range if value(model.b_format[p,j,fb]) > 0.5)
        print(f"  B[{p},{j}] (density={BIN_LABELS[densityB[p,j]]}) -> {format_names[chosen]}")

print(f"\n=== Total cost: {value(model.total_time):.0f} ===")

with open('solution.bin', 'wb') as f:
    f.write(struct.pack('QQQ', M, N, K))

    # A formats
    for i in range(M):
        for p in range(K):
            chosen = next(fa for fa in model.sparse_formats_range
                         if value(model.a_format[i,p,fa]) > 0.5)
            f.write(struct.pack('B', chosen + 1))

    # B formats
    for p in range(K):
        for j in range(N):
            chosen = next(fb for fb in model.sparse_formats_range
                         if value(model.b_format[p,j,fb]) > 0.5)
            f.write(struct.pack('B', chosen + 1))

    # Matrices
    dump_matrix(f, matrixA)
    dump_matrix(f, matrixB)
