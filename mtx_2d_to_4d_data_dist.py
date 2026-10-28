#!/usr/bin/env python3
#
# This program reads in an mtx file for a 2d matrix
# and produces a 4-6d tensor
#
# The program should take in:
# - block size (num_of_elems_per_block)
# - number of processes (num_of_ranks)
#   - row ranks
#   - col rank
# - if the input is in fortran indexing (start at 1) or C (start at 0)
# - if the output is in fortran indexing (start at 1) or C (start at 0)
# - if the output dimension (row, col) needs to be flattened to:
#   - 1d: (rid*(num_of_ranks * num_of_elems_per_block) + bid*num_elems_per_block + eid)
#   - 2d: (rid, bid*num_elems_per_block + eid)
#   - 3d: (rid,bid,eid)
#
#
# See:
# - https://math.nist.gov/MatrixMarket/formats.html
# - https://docs.scipy.org/doc/scipy/reference/generated/scipy.io.mmread.html


from io import StringIO
from scipy.io import mmread
import argparse


# NOTE: This math only works with 0-based (C) indexing
def distribute_block_cyclic_1d(num_of_ranks, num_of_elems_per_block, seq_idx):
#      const int bid = i / (num_of_ranks * num_of_elems_per_block);
#      const int rid = (i % ((num_of_ranks * num_of_elems_per_block)) ) / num_of_elems_per_block;
#      const int eid = i % num_of_elems_per_block;

    bid = seq_idx % num_of_elems_per_block
    rid = int((seq_idx % ((num_of_ranks * num_of_elems_per_block)) ) / num_of_elems_per_block)
    eid = seq_idx % num_of_elems_per_block

    return (rid,bid,eid)

# NOTE This is in Fortran indexing (start at 1 not 0)
# row col data
text = '''%%MatrixMarket matrix coordinate real general
 5 5 7
 2 3 1.0
 3 4 2.0
 3 5 3.0
 4 1 4.0
 4 2 5.0
 4 3 6.0
 4 4 7.0
'''

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("matrix", help="Matrix market file to be distributed.")

    parser.add_argument("--block_size", type=int, default=2,
                        help="TODO")
    parser.add_argument("--row_ranks", type=int, default=2,
                        help="TODO")
    parser.add_argument("--col_ranks", type=int, default=2,
                        help="TODO")

    parser.add_argument("--fortran_input", type=bool, default=True,
                        help = "TODO")
    parser.add_argument("--fortran_output", type=bool, default=True,
                        help = "TODO")

    parser.add_argument("--output_format", type=str, default="debug",
                        help = "TODO")

    args = parser.parse_args()

    m = mmread(args.matrix)
    print(m)

    row_num_of_ranks = args.row_ranks
    row_num_of_elems_per_block = args.block_size

    col_num_of_ranks = args.col_ranks
    col_num_of_elems_per_block = args.block_size

    out_format = args.output_format

    # out_format="3dX3d" # row-format X col-format
    # out_format="1dX1d"
    # out_format="2dX3d"
    # out_format="1dX3d"
    # out_format="3dX2d"
    # out_format="3dX1d"
    # out_format="2dX2d"
    # out_format="1dX2d"
    # out_format="2dX1d"

    for (idx,val) in enumerate(m.data):
        row_idx = m.row[idx]
        col_idx = m.col[idx]
        val     = m.data[idx]

        (row_rid,row_bid,row_eid) = distribute_block_cyclic_1d(row_num_of_ranks,
                                                            row_num_of_elems_per_block,
                                                            row_idx)

        (col_rid,col_bid,col_eid) = distribute_block_cyclic_1d(col_num_of_ranks,
                                                            col_num_of_elems_per_block,
                                                            col_idx)

        # flattened versions of the distribution
        row_idx_1d = (row_rid*(row_num_of_ranks*row_num_of_elems_per_block) +
                    row_bid*row_num_of_elems_per_block + row_eid)

        col_idx_1d = (col_rid*(col_num_of_ranks*col_num_of_elems_per_block) +
                    col_bid*col_num_of_elems_per_block + col_eid)

        row_idx_2d = (row_rid, (row_bid*row_num_of_elems_per_block + row_eid))
        col_idx_2d = (col_rid, (col_bid*col_num_of_elems_per_block + col_eid))

        # TODO: if the option for the output is fortran then all of these output values should be shifted by 1.

        match out_format:
            case "debug":
                print(f"({row_idx},{col_idx},{val}) --> ({row_rid},{row_bid},{row_eid},{col_rid},{col_bid},{col_eid},{val})")
            case "3dX3d":
                print(f"{row_rid} {row_bid} {row_eid} {col_rid} {col_bid} {col_eid} {val}")
            case "2dX3d":
                print(f"{row_idx_2d[0]} {row_idx_2d[0]} {col_rid} {col_bid} {col_eid} {val}")
            case "1dX3d":
                print(f"{row_idx_1d} {col_rid} {col_bid} {col_eid} {val}")

            case "3dX2d":
                print(f"{row_rid} {row_bid} {row_eid} {col_idx_2d[0]} {col_idx_2d[0]} {val}")

            case "3dX1d":
                print(f"{row_rid} {row_bid} {row_eid} {col_idx_1d} {val}")

            case "2dX2d":
                print(f"{row_idx_2d[0]} {row_idx_2d[0]} {col_idx_2d[0]} {col_idx_2d[0]} {val}")

            case "1dX2d":
                print(f"{row_idx_1d} {col_idx_2d[0]} {col_idx_2d[0]} {val}")

            case "2dX1d":
                print(f"{row_idx_2d[0]} {row_idx_2d[0]} {col_idx_1d} {val}")

            case "1dX1d":
                print(f"{row_idx_1d} {col_idx_1d} {val}")

if __name__ == "__main__":
    main()
