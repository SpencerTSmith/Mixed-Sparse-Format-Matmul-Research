import struct
import re
import numpy as np
import sys
from types import SimpleNamespace

def load_edge_list(path):
    k = int(re.search(r'k(\d+)_', path).group(1))
    size = 2 ** k

    rows, cols = [], []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            r, c = line.split()
            rows.append(int(r))
            cols.append(int(c))
    rows_arr = np.array(rows)
    cols_arr = np.array(cols)
    order = np.lexsort((cols_arr, rows_arr))
    return SimpleNamespace(
        row=rows_arr[order],
        col=cols_arr[order],
        data=np.ones(len(rows_arr), dtype=np.float64),
        shape=(size, size)
    )

def dump_matrix(f, matrix):
    size_r, size_c = matrix.shape
    assert size_r <= 65535 and size_c <= 65535, f"Matrix dimensions {size_r}x{size_c} exceed u16 range"


    nnz = len(matrix.data)
    print(size_r, size_c, nnz)
    f.write(struct.pack('III', size_r, size_c, nnz))
    # Planar arrays, indices downcast to u16
    f.write(struct.pack(f'{nnz}H', *matrix.row))
    f.write(struct.pack(f'{nnz}H', *matrix.col))
    f.write(struct.pack(f'{nnz}d', *matrix.data))

matrix = sys.argv[1]


LRC = 32
LCC = 32
RCC = 32


with open("dumped.bin", "wb") as file:
    loaded = load_edge_list(matrix)
    dump_matrix(file, loaded)
    dump_matrix(file, loaded)
