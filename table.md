# Table
- RNZ := right non zero count
- RRC := right row count
- RCC := right col count

- LNZ := left non zero count
- LRC := left row count
- LCC := left col count

| Out Format | Left Format | Right Format | Flop Count         | Memop Count                        |
|------------|-------------|--------------|--------------------|------------------------------------|
|Dense       |Dense        |Dense         |2 * LRC * LCC * RCC |(2 * LRC * LCC * RCC) + (LRC * RCC) |
|Dense       |CSR          |Dense         |2 * LNZ * RCC       |(2 * LRC) + (2 * LNZ) + (3*LNZ*RCC) |
|Dense       |CSC          |Dense         |2 * LNZ * RCC       |(2 * LCC) + (2 * LNZ) + (3*LNZ*RCC) |
|Dense       |CSR          |CSR           |
|Dense       |CSR          |CSC           |
|Dense       |CSC          |CSR           |
|Dense       |CSC          |CSC           |
