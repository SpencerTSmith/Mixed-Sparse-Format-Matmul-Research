# Table
- RNZ := right non zero count
- RRC := right row count
- RCC := right col count

- LNZ := left non zero count
- LRC := left row count
- LCC := left col count

| Out Format | Left Format | Right Format | Flop Count                                 | Memop Count                        |
|------------|-------------|--------------|--------------------------------------------|------------------------------------|
|Dense       |Dense        |Dense         |2 * LRC * LCC * RCC                         |(2 * LRC * LCC * RCC) + (LRC * RCC) |
|Dense       |Dense        |CSR           |                                            |                                    |
|Dense       |Dense        |CSC           |                                            |                                    |
|Dense       |CSR          |Dense         |2 * LNZ * RCC                               |(2 * LRC) + (2 * LNZ) + (3*LNZ*RCC) |
|Dense       |CSR          |CSR           |(2 * LRC) + (4 * LNZ) + (4 * LNZ * RNZ/RCC) |(2 * LNZ * RNZ / RCC)                 | // Average nonzeros per row ~= RNZ / RCC for uniform density?
|Dense       |CSR          |CSC           |                                            |                                    |
|Dense       |CSC          |Dense         |2 * LNZ * RCC                               |(2 * LCC) + (2 * LNZ) + (3*LNZ*RCC) |
|Dense       |CSC          |CSR           |                                            |                                    |
|Dense       |CSC          |CSC           |                                            |                                    |
|CSR         |Dense        |Dense         |                                            |                                    |
|CSR         |Dense        |CSR           |                                            |                                    |
|CSR         |Dense        |CSC           |                                            |                                    |
|CSR         |CSR          |Dense         |                                            |                                    |
|CSR         |CSR          |CSR           |                                            |                                    |
|CSR         |CSR          |CSC           |                                            |                                    |
|CSR         |CSC          |Dense         |                                            |                                    |
|CSR         |CSC          |CSR           |                                            |                                    |
|CSR         |CSC          |CSC           |                                            |                                    |
|CSC         |Dense        |Dense         |                                            |                                    |
|CSC         |Dense        |CSR           |                                            |                                    |
|CSC         |Dense        |CSC           |                                            |                                    |
|CSC         |CSR          |Dense         |                                            |                                    |
|CSC         |CSR          |CSR           |                                            |                                    |
|CSC         |CSR          |CSC           |                                            |                                    |
|CSC         |CSC          |Dense         |                                            |                                    |
|CSC         |CSC          |CSR           |                                            |                                    |
|CSC         |CSC          |CSC           |                                            |                                    |
