import matplotlib.pyplot as plt

ks = [7, 8, 9, 10, 11, 12, 13]
solver_ms = [14199, 62000, 758802, 6916356, 78015690, 971931024, 6581288931]
global_ms = [6888, 7273, 26708, 92069, 539691, 1842620, 6564946]

plt.plot(ks, solver_ms, marker='o', label='Sparse BLIS')
plt.plot(ks, global_ms, marker='o', label='Global CSR x CSR')

plt.xlabel('K (matrix size = 2^K)')
plt.ylabel('Time (ms)')
plt.title('Sparse BLIS vs Global CSR - Kronecker Graph Scaling')
plt.xticks(ks, [f'k={k}\n({2**k}x{2**k})' for k in ks])
plt.legend()
plt.tight_layout()
plt.savefig('kronecker_scaling.png', dpi=150)
