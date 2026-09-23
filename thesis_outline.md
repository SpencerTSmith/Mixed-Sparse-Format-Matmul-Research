# Outline
- Abstract
    - Problem
    - Why do we care?
    - How is it being done today? What is the gap?
    - How I address the gap?
    - Results
- Intro
    - Condense findings, argument may be something like 'format matters more when sparseness has structure that can be exploited, however current hardware may not exploit structure to the fullest'
        - Needs more scoping/specifics
    - Answer and elaborate on abstract questions
        - Speak about specific applications
            - Pagerank
            - https://arxiv.org/html/2508.04077v1 would be good here
            - @techreport{duff1982survey,
                          title={A survey of sparse matrix software},
                          author={Duff, Iain S},
                          year={1982},
                          institution={CM-P00068668}
               }
- Background
    - Sparse matrix matrix
        - Make people care
    - Structured Sparsity
        - Kronecker
        - Moez thesis
    - _In the language of Taco_
        - Formats/data structures
            - Pictures
            - Formats that aren't represented by taco/limitations
                - Non uniform block sizes
                - RMV
                - Recursive Sparse Block
        - Algorithms
            - Contemporary surveys
            - Pictures
    - Hardware acceleration for sparse linear algebra
- Literature Review
    - Other similar papers, how is this different
    - Taco paper for sure
    - Besides that, probably a mixture of papers on the above topics in background
- Formulas for memops/flops from PACT paper
- Repetition Tester benchmark framework
    - Hardware counters
    - Roofline
    - Instrumentation
- Experiments
    - Data sets that interpolate, both varying size and structure
        - Prior work: single point data sets
            - SNAP
            - Florida Sparse Matrix Collection
    - PACT experiments
    - Recent OSCER experiments
- FPGA exploration from HPEC paper
    - Perhaps should go earlier? Happened earlier and with less knowledge
- Future work
    - Focus mostly on further exploration from HPEC paper now have way more knowledge
    - More interpretive data
