- Gustavson's algorithm.... i.e. csr x csr with an accumulator + index buffers for non-zero values
    - Current hardware without good scatter gather can't take full advantage
    - Papers suggest new hardware with fast scatter gather in isa
        - This hardware seems beneficial for moderate sparsity 40-75% non-zeroes specifically
          in transformers
    - A is streamed, only B is reused
        - Hardware papers discuss cache for B, meaning that nearby rows of A would preferably have similar shapes.
            - Can reorganize order of processing rows of A to maximize reuse
                - Connection to structured sparsity, kronecker
    - Outer product seems more suited to denser stuff.
